#ifndef _LINUX_NVPC_H
#define _LINUX_NVPC_H

#include <linux/types.h>
#include <linux/dax.h>
#include <linux/uio.h>
#include <linux/list.h>
#include <linux/migrate.h>
#include <linux/spinlock.h>

#include <linux/nvpc_flag.h>

struct nvpc_opts
{
    struct dax_device *dev;
    int nid;        /* numa node id */
    bool lru;
    bool syn;
    size_t lru_sz;  /* in pages */
    size_t syn_sz;  /* in pages */
    u8 promote_level;
};

struct nvpc
{
    // TODO: make these flages atomic
    bool enabled;
    struct dax_device *dax_dev;
    /* kernel address that the dax_dev is mapped to */
    void *dax_kaddr;
    /* mapped length in pages */
    size_t len_pg;
    /* node id */
    // TODO: set this
    int nid;

    /* add an lru list inside pmem after the inactive list */
    bool extend_lru;
    bool absorb_syn;

    // /* the separator of lru zone and syn zone */
    // void *sep_lru_syn;

    void *lru_begin;
    void *syn_begin;

    size_t lru_sz;
    size_t syn_sz;

    /* free lists: just per-page free list, we don't need buddy here */
    // TODO: should we manage free pages on nvm, or should we do that on dram?
    struct list_head lru_free_list;
    struct list_head syn_free_list;
    /* how many free pages */
    size_t lru_free;
    size_t syn_free;

    /* working lists */
    /* the extension of file-backed lru, after the inactive list */

    /* 
     * promote to DRAM after promote_level accesses 
     * e.g. promote_level=0                     never promote (for high speed nvm)
     *      promote_level=1                     promote at the first access
     *      promote_level=NVPC_LRU_LEVEL_MAX    promote when access time reaches
     *                                          NVPC_LRU_LEVEL_MAX
     */
    u8 promote_level;   /* less than or equal to NVPC_LRU_LEVEL_MAX */

    /*
     * lock order:
     *
     *  meta_lock       --  hold the read lock all the time on any ops
     *                      hold the write lock when updating meta
     *  lru_lock        --  lock lru list
     *  lru_free_lock   --  only when getting free lru page
     *  syn_lock        --  lock syn list
     *  syn_free_lock   --  only when getting free syn page
     * 
     * 
     */
    
    // TODO: reconsider the locks
    rwlock_t meta_lock;
    spinlock_t lru_lock;
    spinlock_t lru_free_lock;
    spinlock_t syn_lock;
    spinlock_t syn_free_lock;
};

extern struct nvpc nvpc;

#define NVPC_ADDR_LOW   (nvpc.dax_kaddr)
#define NVPC_ADDR_HIGH  (nvpc.dax_kaddr + (nvpc.len_pg << PAGE_SHIFT))

int init_nvpc(struct nvpc_opts *opts);
void fini_nvpc(void);
static inline struct nvpc *get_nvpc(void)
{
    // if nvpc is per-node, we should return the one in the local node
    return &nvpc;
}

#ifdef CONFIG_NVPC
static inline bool PageNVPC(struct page *page)
{
    return page_address(page) >= NVPC_ADDR_LOW && page_address(page) < NVPC_ADDR_HIGH;
}

static inline bool nvpc_enabled(void)
{
    return nvpc.enabled;
}
#else
#define PageNVPC(page) false
#define nvpc_enabled() false
#endif

/* get the address at an offset of nvpc */
static inline void *nvpc_get_addr(loff_t off)
{
    return nvpc.dax_kaddr + off;
}

static inline void *nvpc_get_addr_pg(loff_t off_pg)
{
    return nvpc.dax_kaddr + (off_pg << PAGE_SHIFT);
}

/* copy data from kernel to nvpc */
void nvpc_write_nv(void *from, loff_t off, size_t len);
/* copy data from nvpc to kernel */
void nvpc_read_nv(void *to, loff_t off, size_t len);

/* copy data from user to nvpc */
size_t nvpc_write_nv_iter(struct iov_iter *from, loff_t off, bool flush);
// size_t nvpc_write_nv_iter_noflush(struct iov_iter *from, loff_t off, size_t len);
/* copy data from nvpc to user */
size_t nvpc_read_nv_iter(struct iov_iter *to, loff_t off);

static inline void nvpc_wmb(void) {
    pmem_wmb();
}


void nvpc_lru_size(size_t *free, size_t *total);
void nvpc_syn_size(size_t *free, size_t *total);

// void nvpc_lru_init();
// /* copy pages from mem to nvpc lru zone */
// void nvpc_lru_add_pg_list(struct list_head *page_list);



/* 
 * get a new page when inactive lru migrate to nvpc lru
 * callee hold lru_free_lock
 */
struct page *nvpc_get_new_page(struct page *page, unsigned long private);
void nvpc_free_page(struct page *page, unsigned long private);

/* alloc dram page when nvpc page promote back */
struct page *nvpc_alloc_promote_page(struct page *page, unsigned long node);

#endif