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
#include <linux/mm_inline.h>

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

/* vector that temporarily stores NVPC pages that reach the promote level */
static struct page *promote_vec[NVPC_PROMOTE_VEC_SZ];
static atomic_t nr_promote_vec;
int promote_order = order_base_2(NVPC_PROMOTE_VEC_SZ);

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

        /* turn the refcnt of page into 0 and add to free list */
        // pr_info("[NVPC TEST]: init0 ref=%d\n", page_count(pg));
        // put_page(pg);
        page_ref_dec(pg);
        list_add(&pg->lru, l);
    }

    return i;
}

/* sizes are in pages */
int __ref init_nvpc(struct nvpc_opts *opts)
{
    pfn_t pfn;
    unsigned long irq_flags;
    support_clwb = static_cpu_has(X86_FEATURE_CLWB);

    rwlock_init(&nvpc.meta_lock);
    spin_lock_init(&nvpc.lru_lock);
    spin_lock_init(&nvpc.syn_lock);
    spin_lock_init(&nvpc.nvpc_free_lock);

    // NVTODO: do we need to lock anything here?

    nvpc.dax_dev = opts->dev;
    nvpc.nid = opts->nid;
    nvpc.promote_level = opts->promote_level;

    nvpc.len_pg = dax_map_whole_dev(nvpc.dax_dev, &nvpc.dax_kaddr, &pfn);
    nvpc.pfn = pfn_t_to_pfn(pfn);

    if (opts->extend_lru && opts->nvpc_sz > nvpc.len_pg)
    {
        pr_info("NVPC init: nvpc_sz is larger than the available length of dax device, using the whole device for NVPC.\n");
        opts->nvpc_sz = nvpc.len_pg;
    }

    nvpc.extend_lru = opts->extend_lru;
    nvpc.absorb_syn = opts->absorb_syn;

    nvpc.nvpc_free_pgnum = nvpc.nvpc_sz = opts->nvpc_sz;
    nvpc.warning_pgnum = max(nvpc.nvpc_sz / 100, (size_t)1) * NVPC_EVICT_WATERMARK;
    nvpc.evict_order   = order_base_2(max(nvpc.nvpc_sz / 100, (size_t)1) * NVPC_EVICT_PERCENT);

    /* init promote vec */
    atomic_set(&nr_promote_vec, 0);

    /* page ref count is 1 after init */
    memmap_init_range(nvpc.nvpc_sz, nvpc.nid, ZONE_NORMAL, nvpc.pfn, 0, MEMINIT_HOTPLUG, NULL, MIGRATE_ISOLATE);
    
    nvpc.nvpc_begin = nvpc_get_addr_pg(0);

    INIT_LIST_HEAD(&nvpc.nvpc_free_list);

    spin_lock_irqsave(&nvpc.nvpc_free_lock, irq_flags);
    /* page ref count is dropped to 0 here */
    __init_free_list(&nvpc.nvpc_free_list, 0, nvpc.nvpc_sz);
    spin_unlock_irqrestore(&nvpc.nvpc_free_lock, irq_flags);

    nvpc.enabled = true;
    pr_info("NVPC init: NVPC started with %zu pages.\n", nvpc.nvpc_sz);

    return 0;
}

void fini_nvpc(void)
{
    /* don't need to lock */
    nvpc.enabled = false;
}

void nvpc_get_usage(size_t *free, size_t *syn_usage, size_t *total)
{
    *free = nvpc.nvpc_free_pgnum;
    *syn_usage = nvpc.syn_used_pgnum;
    *total = nvpc.nvpc_sz;
}

/* 
 * 
 * page reference count is set to 1
 * 
 */
struct page *nvpc_get_new_page(struct page *page, unsigned long private)
{
    /* we don't care about the old page, caller ensures that it's not a huge page */
    unsigned long irq_flags;
    struct page *newpage;
    struct list_head *list;
    spinlock_t *lock;
    bool should_evict = false;

    list = &nvpc.nvpc_free_list;
    lock = &nvpc.nvpc_free_lock;
    // pr_info("[NVPC TEST]: nvpc_get_new_page @cpu%d\n", smp_processor_id());
    spin_lock_irqsave(lock, irq_flags);

    if (list_empty(list))
    {
        should_evict = true;
        newpage = NULL;
        goto out;
    }

    nvpc.nvpc_free_pgnum--;
    should_evict = nvpc.nvpc_free_pgnum <= nvpc.warning_pgnum;

    newpage = list_last_entry(list, struct page, lru);
    list_del(&newpage->lru);
    page_ref_inc(newpage);
    page_nvpc_lru_cnt_set(newpage, 0);
    ClearPageReserved(newpage);
    // pr_info("[NVPC TEST]: nvpc_get_new_page new=%p page_reserved=%d @cpu%d\n", newpage, PageReserved(newpage), smp_processor_id());

out:
    spin_unlock_irqrestore(lock, irq_flags);

    if (should_evict)
        nvpc_wakeup_nvpc_evict();

    return newpage;
}

// NVTODO: we can apply per_cpu_pages (pcp) bulk free here
void nvpc_free_page(struct page *page, unsigned long private)
{
    unsigned long irq_flags;
    struct list_head *list;
    spinlock_t *lock;
    list = &nvpc.nvpc_free_list;
    lock = &nvpc.nvpc_free_lock;
    // pr_info("[NVPC TEST]: nvpc_free_page pg=%p page_reserved=%d @cpu%d\n", page, PageReserved(page), smp_processor_id());
    
    spin_lock_irqsave(lock, irq_flags);

    // NVTODO: do some clean up here

    if (TestSetPageReserved(page)) {
        /* if page is already returned (reserved is set), just quit */
        goto out;
    }

    /* reset page reference count to 0 */
    init_page_count(page);
    page_ref_dec(page);

    nvpc.nvpc_free_pgnum++;
    list_add(&page->lru, list);
    // pr_info("[NVPC TEST]: nvpc_free_page nvpc_free_pgnum=%zu @cpu%d\n", nvpc.nvpc_free_pgnum, smp_processor_id());
out:
    spin_unlock_irqrestore(lock, irq_flags);
}

// NVXXX: big problem here... deprecated
void nvpc_free_lru_page(struct page *page)
{
    unsigned long flags;
    struct lruvec *lruvec = NULL;
    /*
     * 2 references are holded: 
     * lru is holding 1, nvpc is holding 1
     */

    if (!trylock_page(page))
        return;

    if (!TestClearPageLRU(page)) 
        goto out;

    put_page(page);
    if (!put_page_testzero(page))
    {
        page_ref_add(page, 2);
        SetPageLRU(page);
        goto out;
    }
    
    lruvec = lock_page_lruvec_irqsave(page, &flags);
    del_page_from_lru_list(page, lruvec);
    unlock_page_lruvec_irqrestore(lruvec, flags);

    nvpc_free_page(page, 0); 

out:
    unlock_page(page);
}

void nvpc_free_pages(struct list_head *list)
{
    struct page *page, *next;
    list_for_each_entry_safe(page, next, list, lru) {
        if (!PageNVPC(page))
            continue;
        
        list_del(&page->lru);
        // NVTODO: if page is in persistance domain, do not free!!!
        nvpc_free_page(page, 0);
    }
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
        // NVTODO: if can't get a page here, just fucking return
        .gfp_mask = GFP_HIGHUSER_MOVABLE | __GFP_RECLAIM |
			    __GFP_THISNODE | __GFP_NOWARN |
			    __GFP_NOMEMALLOC | GFP_NOWAIT,
		.nid = nvpc.nid
	};

	return alloc_migration_target(page, (unsigned long)&mtc);
}

/* return the old val if success, return -1 if fail */
static inline int atomic_fetch_inc_test_upperbound(atomic_t *val, int upper)
{
    int old, new;
    old = atomic_read(&nr_promote_vec);

    do
    {
        if (old >= upper)
            return -1;
        new = old+1;
    } while (!atomic_try_cmpxchg(val, &old, new));

    return old;
}

/* return number of pages in promote vec */
int nvpc_promote_vec_put_page(struct page * page)
{
    int idx = atomic_fetch_inc_test_upperbound(&nr_promote_vec, NVPC_PROMOTE_VEC_SZ);
    
    if (idx < 0)
        return -1;

    promote_vec[idx] = page;
    return idx+1;
}

// NVTODO: periodically do this
/* clear the vec and the access count of its members */
void nvpc_promote_vec_clear()
{
    int i;
    int num = atomic_read(&nr_promote_vec);
    /* refuse other processes to update the promote vec */
    atomic_set(&nr_promote_vec, NVPC_PROMOTE_VEC_SZ);

    for (i = 0; i < num; i++)
    {
        page_nvpc_lru_cnt_set(promote_vec[i], 0);
    }
    atomic_set(&nr_promote_vec, 0);
}

/* caller must ensure that this function can manipulate NVPC lru!!! */
int nvpc_promote_vec_isolate(struct list_head *page_list)
{
    int i;
    int num = atomic_read(&nr_promote_vec);
    /* refuse other processes to update the promote vec */
    atomic_set(&nr_promote_vec, NVPC_PROMOTE_VEC_SZ);

    for (i = 0; i < num; i++)
    {
        list_move(&promote_vec[i]->lru, page_list);
    }
    
    atomic_set(&nr_promote_vec, 0);
    return num;
}

int nvpc_promote_vec_nr()
{
    return atomic_read(&nr_promote_vec);
}

// bool nvpc_should_promote()
// {
//     /*
//      * 1) if nvpc promote vec is full, promote
//      * 
//      * 2) if nvpc promote vec page number is less than
//      *    dram unreached page number, promote
//      */
//     int n_pv_num = atomic_read(&nr_promote_vec);
//     int d_ur_num;   // NVTODO: track this /* use inactive list !PageReferenced page num */

//     return n_pv_num == PROMOTE_VEC_SZ || n_pv_num < d_ur_num;
// }

void nvpc_wakeup_nvpc_promote(pg_data_t *pgdat)
{
    // wakeup_kswapd();

    /* 
     * wake up kswapd on node 0 
     * 
     * pgdat->kswapd_highest_zoneidx is set to ZONE_NORMAL;
     * pgdat->kswapd_order is set to the order of pages 
     * prepared to promote;
     * when kswapd runs, it will shrink dram lru lists to 
     * prepare #order pages on ZONE_NORMAL for promotion
     */
    enum zone_type curr_idx;
    // pg_data_t *pgdat = NODE_DATA(0);

    if (!waitqueue_active(&pgdat->kswapd_wait))
		return;

    curr_idx = READ_ONCE(pgdat->kswapd_highest_zoneidx);

	if (curr_idx == MAX_NR_ZONES || curr_idx < ZONE_NORMAL)
		WRITE_ONCE(pgdat->kswapd_highest_zoneidx, ZONE_NORMAL);

	if (READ_ONCE(pgdat->kswapd_order) < promote_order)
		WRITE_ONCE(pgdat->kswapd_order, promote_order);

    // trace_mm_vmscan_wakeup_kswapd(0, ZONE_NORMAL, 0,
	// 			      0);
	// wake_up_interruptible(&pgdat->kswapd_wait);
}

void nvpc_wakeup_nvpc_evict()
{
    // NVTODO: wait for knvpcd to finish
    // pg_data_t *pgdat = NODE_DATA(0);
    // enum zone_type curr_idx;

    // if (!waitqueue_active(&pgdat->knvpcd_wait))
	// 	return;

	// if (READ_ONCE(pgdat->knvpcd_order) < nvpc.evict_order)
	// 	WRITE_ONCE(pgdat->knvpcd_order, nvpc.evict_order);
    
    // wake_up_interruptible(&pgdat->knvpcd_wait);
}


// NVTODO: debug, remove this
int debug_print = 0;
EXPORT_SYMBOL_GPL(debug_print);
