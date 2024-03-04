#ifndef _LINUX_NVPC_BASE_H
#define _LINUX_NVPC_BASE_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

struct nvpc
{
    // NVTODO: make these flages atomic
    bool enabled;
    struct dax_device *dax_dev;
    /* kernel address that the dax_dev is mapped to */
    void *dax_kaddr;
    unsigned long pfn;
    /* mapped length in pages */
    size_t len_pg;
    /* node id */
    int nid;

    // Add a LRU list in NVM after DRAM inactive list
    // Also known as the NVPC Demotion (by kswapd)
    bool extend_lru;

    // Enable SYN absorption, which means that the persistent 
    // pages will be absorbed into NVPC (NVM) rather than original blockdev
    bool absorb_syn;

    bool active_sync;
    
    /* 
     * promote to DRAM after promote_level accesses 
     * e.g. promote_level=0                     never promote (for high speed nvm)
     *      promote_level=1                     promote at the first access
     *      promote_level=NVPC_LRU_LEVEL_MAX    promote when access time reaches
     *                                          NVPC_LRU_LEVEL_MAX
     */
    u8 promote_level;   /* less than or equal to NVPC_LRU_LEVEL_MAX */

    // Enable NVPC eviction, which means that the pages in NVPC
    // can be directly evicted, but dirty pages are processed by SYN subsystem
    bool nvpc_lru_evict;

    // Before promotion, we need to check whether we have enough free pages in DRAM
    // However, when we need to promote pages, there are rarely free pages in DRAM
    // So wake up kswapd to get more free pages
    bool demote_before_promote;

    // Temporarily enable NVPC daemon to process extended LRU list
    // we Also called knvpcd
    // Do not modify this flag directly, use wakeup_knvpcd()
    unsigned int knvpcd_should_run;

    // For knvpcd wakeup and sleep, do not use this directly
    wait_queue_head_t knvpcd_wait;

    // NVXXX: memalloc is managed by free_nvpc_list, so deprecated
    // wait_queue_head_t pmemalloc_wait;
    
    struct task_struct *knvpcd;

    unsigned long knvpcd_nr_to_promote;
    unsigned long knvpcd_nr_to_reclaim;

    // NVTODO: function undone
    // atomic_long_t knvpcd_failures; // number of demotes || evictions || promoted == 0 returns

    // /* the separator of lru zone and syn zone */
    // void *sep_lru_syn;

    void *nvpc_begin;
    // void *syn_begin;

    size_t nvpc_sz;
    // size_t syn_sz;
    size_t warning_pgnum;
    size_t evict_order;

    /* free lists: just per-page free list, we don't need buddy here */
    // NVTODO: add some bits in page->flags, so that we don't need to seperate lru and syn here
    struct list_head nvpc_free_list;
    // struct list_head syn_free_list;
    /* how many free pages */
    size_t nvpc_free_pgnum;
    // size_t syn_free;
    size_t syn_used_pgnum;

    /* working lists */
    /* the extension of file-backed lru, after the inactive list */


    /*
     * lock order:
     *
     *  meta_lock       --  hold the read lock all the time on any ops
     *                      hold the write lock when updating meta
     *  lru_lock        --  lock lru list
     *  lru_free_lock   --  only when getting free lru page
     *  syn_lock        --  lock syn list
     *  syn_free_lock   --  only when getting free syn page
     */
    
    // NVTODO: reconsider the locks
    rwlock_t meta_lock;
    spinlock_t nvpc_free_lock;
    spinlock_t lru_lock;
    // spinlock_t lru_free_lock;
    spinlock_t syn_lock;
    // spinlock_t syn_free_lock;


};

extern struct nvpc nvpc;

// #ifdef CONFIG_NVPC
// static inline bool PageNVPC(struct page *page);
// static inline bool nvpc_enabled(void);
#ifndef CONFIG_NVPC
#define PageNVPC(page) false
#define nvpc_enabled() false
#endif

#endif