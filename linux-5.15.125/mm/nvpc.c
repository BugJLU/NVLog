/*
 * NVPC: A page cache extention with non-volatile memory.
 * This is a prototype and may be unsafe now.
 * 
 * NVM is used as an add-on to the normal DRAM page cache
 * to speed up the sync writes and expand the size of the
 * lru of page cache (file backed).
 * 
 * NOTE: NVPC uses the whole NVM device, so a device should 
 * be used exclusively as the device of NVPC and should not 
 * be accessed via any other method (like fs). 
 */


#include <linux/nvpc.h>
#include <linux/uio.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/local_lock.h>
#include <linux/percpu.h>
#include <linux/percpu_counter.h>
#include <linux/pfn_t.h>
#include <linux/memcontrol.h>

#include "internal.h"

/* NVPC is global, use get_nvpc to get the pointer to this instance */
struct nvpc nvpc = {
    .enabled = false, 
    .extend_lru = false,
    .absorb_syn = false, 
    .promote_level = 4
};
EXPORT_SYMBOL_GPL(nvpc);

static bool support_clwb;

/*
 * l: the list
 * begin_pg: the begining page index, relative in the dax device
 * sz_pg: how many pages
 * 
 * return: the next free page index
 */
static size_t __init_free_list(struct list_head *l, loff_t begin_pg, size_t sz_pg)
{
    size_t i;
    void *addr;
    struct page *pg;

    /* init lru or syn free list */
    for (i = begin_pg; i < begin_pg+sz_pg; i++) {
        addr = nvpc_get_addr_pg(i);
        pg = virt_to_page(addr);

        /* turn the refcnt of page into 2 */
        get_page(pg);
        list_add(&pg->lru, l);
    }

    return i;
}

/* sizes are in pages */
int init_nvpc(struct nvpc_opts *opts)
{
    loff_t lru_idx, syn_idx;
    pfn_t pfn;
    unsigned long irq_flags;
    support_clwb = static_cpu_has(X86_FEATURE_CLWB);

    rwlock_init(&nvpc.meta_lock);
    spin_lock_init(&nvpc.lru_lock);
    spin_lock_init(&nvpc.lru_free_lock);
    spin_lock_init(&nvpc.syn_lock);
    spin_lock_init(&nvpc.syn_free_lock);

    // NVTODO: do we need to lock anything here?

    nvpc.dax_dev = opts->dev;
    nvpc.nid = opts->nid;
    nvpc.promote_level = opts->promote_level;
    // NVXXX: maybe we don't really need to map the whole device into kaddr?

    nvpc.len_pg = dax_map_whole_dev(nvpc.dax_dev, &nvpc.dax_kaddr, &pfn);
    nvpc.pfn = pfn_t_to_pfn(pfn);

    if (opts->lru_sz + opts->syn_sz > nvpc.len_pg)
        return -EINVAL;

    nvpc.extend_lru = opts->lru;
    nvpc.absorb_syn = opts->syn;

    nvpc.lru_free = nvpc.lru_sz = opts->lru_sz;
    nvpc.syn_free = nvpc.syn_sz = opts->syn_sz;
    
    INIT_LIST_HEAD(&nvpc.lru_free_list);
    INIT_LIST_HEAD(&nvpc.syn_free_list);
    lru_idx = 0;

    spin_lock_irqsave(&nvpc.lru_free_lock, irq_flags);
    syn_idx = __init_free_list(&nvpc.lru_free_list, lru_idx, opts->lru_sz);
    spin_unlock_irqrestore(&nvpc.lru_free_lock, irq_flags);

    spin_lock_irqsave(&nvpc.syn_free_lock, irq_flags);
    __init_free_list(&nvpc.syn_free_list, syn_idx, opts->syn_sz);
    spin_unlock_irqrestore(&nvpc.syn_free_lock, irq_flags);

    nvpc.lru_begin = nvpc_get_addr_pg(lru_idx);
    nvpc.syn_begin = nvpc_get_addr_pg(syn_idx);

    nvpc.enabled = true;

    return 0;
}

void fini_nvpc(void)
{
    /* don't need to lock */
    nvpc.enabled = false;
}

/* copy data to nvpc */
void nvpc_write_nv(void *from, loff_t off, size_t len)
{
    /* movnt for bulk, clwb for residue on x86*/
    memcpy_flushcache(nvpc.dax_kaddr+off, from, len);
}

/* copy data from nvpc */
void nvpc_read_nv(void *to, loff_t off, size_t len)
{
    memcpy(to, nvpc.dax_kaddr+off, len);
}

/* 
 * return number of bytes copied 
 */
static inline size_t _nvpc_write_nv_iter(struct iov_iter *from, loff_t off, size_t len)
{
    // dax_copy_from_iter(nvpc.dax_dev, 0, nvpc.dax_kaddr + off, len, from);
    /* movnt for bulk, clwb for residue on x86*/
    return _copy_from_iter_flushcache(nvpc.dax_kaddr + off, len, from);
}

/* 
 * copy data from iov to nvpc, without cache flush
 * return number of bytes copied
 */
static inline size_t _nvpc_write_nv_iter_noflush(struct iov_iter *from, loff_t off, size_t len)
{
    // NVTODO: evaluate the performance between this and nvpc_write_nv_iter()
    /*
     * In NVPC sometimes we don't care about the persistency of the data, like 
     * when we are dealing with the lru. This function can be used to provide a
     * higher speed...?
     */
    return _copy_from_iter(nvpc.dax_kaddr + off, len, from);
}

/* 
 * return number of bytes copied
 */
static inline size_t _nvpc_read_nv_iter(struct iov_iter *to, loff_t off, size_t len)
{
    // dax_copy_to_iter(nvpc.dax_dev, 0, nvpc.dax_kaddr + off, len, to);
    return _copy_mc_to_iter(nvpc.dax_kaddr + off, len, to);
}

/*
 * copy data from iov to nvpc
 * if flush == true, cache will be flushed after copy
 * return number of bytes copied
 */
size_t nvpc_write_nv_iter(struct iov_iter *from, loff_t off, bool flush)
{
    size_t len;

    len = iov_iter_count(from);
    if (off + len > (nvpc.len_pg << PAGE_SHIFT))
    {
        len = (nvpc.len_pg << PAGE_SHIFT) - off;
    }
    
    pr_debug("Libnvpc: prepared write len %zu\n", len);
    if (flush)
        len = _nvpc_write_nv_iter(from, off, len);
    else
        len = _nvpc_write_nv_iter_noflush(from, off, len);
    pr_debug("Libnvpc: actual write len %zu\n", len);
    
    return len;
}

/* 
 * copy data from nvpc to iov 
 * return number of bytes copied
 */
size_t nvpc_read_nv_iter(struct iov_iter *to, loff_t off)
{
    size_t len;

    len = iov_iter_count(to);
    if (off + len > (nvpc.len_pg << PAGE_SHIFT))
    {
        len = (nvpc.len_pg << PAGE_SHIFT) - off;
    }
    pr_debug("Libnvpc: prepared read len %zu\n", len);
    len = _nvpc_read_nv_iter(to, off, len);
    pr_debug("Libnvpc: actual read len %zu\n", len);

    return len;
}

void nvpc_lru_size(size_t *free, size_t *total)
{
    *total = nvpc.lru_sz;
    *free = nvpc.lru_free;
}
void nvpc_syn_size(size_t *free, size_t *total)
{
    *total = nvpc.syn_sz;
    *free = nvpc.syn_free;
}

/* 
 * caller should hold meta_lock as a read lock 
 * 
 * private = 0: lru_free_list
 * private = 1: syn_free_list
 */
struct page *nvpc_get_new_page(struct page *page, unsigned long private)
{
    // NVTODO: maybe we can use __init_single_page()
    /* we don't care about the old page, caller ensures that it's not a huge page */
    unsigned long irq_flags;
    struct page *newpage;
    struct list_head *list;
    spinlock_t *lock;
    list = private ? &nvpc.syn_free_list : &nvpc.lru_free_list;
    lock = private ? &nvpc.syn_free_lock : &nvpc.lru_free_lock;
    spin_lock_irqsave(lock, irq_flags);

    if (list_empty(list))
    {
        newpage = NULL;
        goto out;
    }

    if (private) nvpc.syn_free--;
    else nvpc.lru_free--;
    newpage = list_last_entry(list, struct page, lru);
    list_del(&newpage->lru);

out:
    spin_unlock_irqrestore(lock, irq_flags);
    if (newpage) {
        /* setup the page */
        page_nvpc_lru_cnt_set(newpage, 0);
        get_page(newpage);
    }
    
    return newpage;
}

// NVTODO: we can apply per_cpu_pages here
void nvpc_free_page(struct page *page, unsigned long private)
{
    unsigned long irq_flags;
    struct list_head *list;
    spinlock_t *lock;
    list = private ? &nvpc.syn_free_list : &nvpc.lru_free_list;
    lock = private ? &nvpc.syn_free_lock : &nvpc.lru_free_lock;
    put_page(page);
    spin_lock_irqsave(lock, irq_flags);

    if (private) nvpc.syn_free++;
    else nvpc.lru_free++;
    list_add(&page->lru, list);

    spin_unlock_irqrestore(lock, irq_flags);
}

/* derived from vmscan.c: alloc_demote_page() */
struct page *nvpc_alloc_promote_page(struct page *page, unsigned long private)
{
    struct migration_target_control mtc = {
        /*
         * Allocate from DRAM of this node, enable reclaim because
         * the page we are promoting is accessed more frequently than
         * those pages in inactive list. 
         */
        // NVXXX: set GFP_NOIO so that only clean pages can be reclaimed?
        .gfp_mask = GFP_HIGHUSER_MOVABLE | __GFP_RECLAIM |
			    __GFP_THISNODE | __GFP_NOWARN |
			    __GFP_NOMEMALLOC,
		.nid = nvpc.nid
	};

	return alloc_migration_target(page, (unsigned long)&mtc);
}