#include <linux/fs.h>
#include <linux/libnvdimm.h>
#include <linux/prefetch.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/blkdev.h>
#include <linux/pagevec.h>
#include <linux/nvpc_rw.h>
#include <linux/nvpc_sync.h>
#include <linux/nvpc.h>

// #define pr_debug pr_info

struct nvpc_sync nvpc_sync;

int nvpc_sync_compact_thread_fn(void *data);

void init_sync_absorb_area(void)
{
    nvpc_sync.super_log_0 = nvpc_get_addr_pg(0);
    // pr_debug("[NVPC DEBUG]: sl0: %#llx\n", nvpc_sync.super_log_0);

    memset(nvpc_sync.super_log_0, 0, PAGE_SIZE);
    ((first_head_entry*)(nvpc_sync.super_log_0))->magic = NVPC_LOG_HEAD_MAGIC;
    arch_wb_cache_pmem(nvpc_sync.super_log_0, PAGE_SIZE);
    nvpc_write_commit();

    nvpc_sync.nvpc_sync_wq = create_workqueue("nvpc_sync_wq");
    if (!nvpc_sync.nvpc_sync_wq)
    {
        pr_err("[NVPC ERROR]: cannot create nvpc sync workqueue\n");
    }

#if defined(NVPC_COMPACT_DAEMON_ON)
    nvpc_sync.compact_interval = NVPC_COMPACT_INTERVAL_DEFAULT;
    nvpc_sync.nvpc_sync_compact_thread = kthread_run(nvpc_sync_compact_thread_fn, NULL, "NVPC COMPACT THREAD");
    if (!nvpc_sync.nvpc_sync_compact_thread)
    {
        pr_err("[NVPC ERROR]: cannot start nvpc compaction thread\n");
    }
#else
    nvpc_sync.nvpc_sync_compact_thread = NULL;
#endif

    mutex_init(&nvpc_sync.super_log_lock);
}

void fini_sync(void)
{
    if (nvpc_sync.nvpc_sync_compact_thread)
    {
        kthread_stop(nvpc_sync.nvpc_sync_compact_thread);
    }
    destroy_workqueue(nvpc_sync.nvpc_sync_wq);
}

typedef bool(*log_head_walker)(nvpc_sync_head_entry *, void *);

/* walk through super log without lock */
static nvpc_sync_head_entry *walk_log_heads(log_head_walker it, void *opaque)
{
    void *current_superlog;
    nvpc_sync_head_entry* log_head_i; // = (nvpc_sync_head_entry*)current_superlog;

    if (likely(nvpc_sync.super_log_0))
        current_superlog = nvpc_sync.super_log_0;
    else
        current_superlog = nvpc_get_addr_pg(0); // for rebuild

    // pr_debug("[NVPC DEBUG]: walk_log_heads entsz: %ld\n", sizeof(nvpc_sync_head_entry));
    // pr_debug("[NVPC DEBUG]: walk_log_heads sl0: %#llx\n", nvpc_sync.super_log_0);

    /* first entry is for magic num */
    log_head_i = (nvpc_sync_head_entry*)current_superlog + 1;
    // pr_debug("[NVPC DEBUG]: walk_log_heads ent1: %#llx\n", log_head_i);

walk_page:
    // pr_debug("[NVPC DEBUG]: walk_log_heads ---page--- upper: %#llx\n", current_superlog + PAGE_SIZE - sizeof(nvpc_sync_head_entry));
    while ((void*)log_head_i < 
           current_superlog + PAGE_SIZE - sizeof(nvpc_sync_head_entry))
    {
        // pr_debug("[NVPC DEBUG]: walk_log_heads lhi: %#llx\n", log_head_i);
        if (log_head_i->flags != NVPC_LOG_HEAD_FLAG_EMPTY)
            if (it(log_head_i, opaque))
                return log_head_i;

        log_head_i++;
    }

    if (NVPC_SL_HAS_NEXT(current_superlog))
    {
        current_superlog = NVPC_SL_NEXT(current_superlog);
        log_head_i = (nvpc_sync_head_entry*)current_superlog;
        goto walk_page;
    }
    
    return NULL;
}

/* get a new page from nvpc free list, then set 0 to it */
static void *create_new_log_page(void)
{
    void *kaddr;
    struct page *newpage = nvpc_get_new_page(NULL, 0);
    if (!newpage)
    {
        return NULL;
    }
    
    kaddr = page_to_virt(newpage);
    memset(kaddr, 0, PAGE_SIZE);
    arch_wb_cache_pmem(kaddr, PAGE_SIZE);
    // nvpc_write_commit();
    return kaddr;
}

static size_t create_n_new_log_pages(struct list_head *pages, size_t n)
{
    void *kaddr;
    struct page *page;
    
    size_t n_get = nvpc_get_n_new_page(pages, n);
    if (n_get < n)
    {
        nvpc_free_pages(pages);
        return 0;
    }
    
    list_for_each_entry(page, pages, lru) {
        kaddr = page_to_virt(page);
        memset(kaddr, 0, PAGE_SIZE);
        arch_wb_cache_pmem(kaddr, PAGE_SIZE);
    }
    return n_get;
}

/* get an empty log head from superlogs. no lock */
static nvpc_sync_head_entry *get_log_head_empty(void)
{
    void *current_superlog = nvpc_sync.super_log_0;
    nvpc_sync_head_entry* log_head_i; // = (nvpc_sync_head_entry*)current_superlog;

    /* first entry is for magic num */
    log_head_i = (nvpc_sync_head_entry*)current_superlog + 1;

walk_page:
    while ((void*)log_head_i < 
           current_superlog + PAGE_SIZE - sizeof(nvpc_sync_head_entry))
    {
        if (log_head_i->flags == NVPC_LOG_HEAD_FLAG_EMPTY)
            return log_head_i;
        
        log_head_i++;
    }

    if (NVPC_SL_HAS_NEXT(current_superlog))
    {
        current_superlog = NVPC_SL_NEXT(current_superlog);
        log_head_i = (nvpc_sync_head_entry*)current_superlog;
        goto walk_page;
    }

    // else: create a new sl, and return the log head on the new sl
    else
    {
        next_sl_page_entry* ne = NVPC_SL_ENTRY_NEXT(current_superlog);
        current_superlog = create_new_log_page();
        ne->next_sl_page = nvpc_get_off_pg(current_superlog);
        if (!current_superlog)
        {
            return NULL;
        }
        ne->flags = NVPC_LOG_HEAD_FLAG_NEXT;
        
        arch_wb_cache_pmem((void*)ne, sizeof(next_sl_page_entry));
        // nvpc_write_commit();

        return (nvpc_sync_head_entry *)current_superlog;
    }
}

/* walker to find a log head for an inode */
static bool __find_log_inode_head_wkr(nvpc_sync_head_entry *ent, void *opaque)
{
    struct inode *ino = (struct inode *)opaque;
    log_inode_head_entry* lent = (log_inode_head_entry*)ent;
    return lent->s_dev == ino->i_sb->s_bdev->bd_dev && lent->i_ino == ino->i_ino;
}

/* create the log and set up the log head */
static int init_log_inode_head(log_inode_head_entry * loghead, struct inode *inode)
{
    void *newlogpg;

    // NVTODO: ihold inode
    
    newlogpg = create_new_log_page();
    loghead->head_log_page = nvpc_get_off_pg(newlogpg);
    if (!newlogpg)
    {
        return -1;
    }
    loghead->s_dev = inode->i_sb->s_bdev->bd_dev;
    loghead->i_ino = inode->i_ino;
    pr_debug("[NVPC DEBUG]: nvpc init_log_inode_head dev %u:%u ino %ld. \n", MAJOR(loghead->s_dev), MINOR(loghead->s_dev), loghead->i_ino);
    /* first entry in the new page */
    loghead->committed_log_tail = (nvpc_sync_log_entry *)newlogpg;
    
    arch_wb_cache_pmem((void*)loghead, sizeof(log_inode_head_entry));
    // nvpc_write_commit();
    return 0;
}

/*
 * create_log_head()
 * Walk through the superlog pages, find if the log head of inode 
 * is already in superlog. If not, alloc a new log_head.
 * Returns the log head of the inode.
 */
static log_inode_head_entry *create_log_head(struct inode *inode)
{
    log_inode_head_entry *loghead;

    if (mutex_lock_interruptible(&nvpc_sync.super_log_lock) != 0)
    {
        return NULL;
    }

    loghead = (log_inode_head_entry *)walk_log_heads(__find_log_inode_head_wkr, (void*)inode);
    // pr_debug("[NVPC DEBUG]: walk_log_heads result: %#llx\n", loghead);
    if (loghead)
        goto out;
    
    loghead = (log_inode_head_entry *)get_log_head_empty();
    // pr_debug("[NVPC DEBUG]: get_log_head_empty result: %#llx\n", loghead);
    if (!loghead)
        goto out;
    
    if (init_log_inode_head(loghead, inode)) {
        /* for get_log_head_empty */
        nvpc_write_commit();
        mutex_unlock(&nvpc_sync.super_log_lock);
        return NULL;
    }
    
    nvpc_write_commit();
out:
    mutex_unlock(&nvpc_sync.super_log_lock);
    return loghead;
}

log_inode_head_entry *nvpc_get_log_inode(struct inode *inode)
{
    // check once first to avoid locking
    if (unlikely(!inode->nvpc_sync_ilog.log_head))
    {
        spin_lock(&inode->i_lock);  // may race on creating log_head
        if (!inode->nvpc_sync_ilog.log_head)    // only once this should fail
        {
            log_inode_head_entry *lh;
            spin_unlock(&inode->i_lock);
            lh = create_log_head(inode);
            if (!lh)
                return NULL;
            
            spin_lock(&inode->i_lock);
            inode->nvpc_sync_ilog.log_head = lh;
            inode->nvpc_sync_ilog.log_tail = lh->committed_log_tail;
            inode->nvpc_sync_ilog.latest_logged_attr = NULL;
        }
        spin_unlock(&inode->i_lock);
    }
    return inode->nvpc_sync_ilog.log_head;
}

typedef bool(*log_walker)(nvpc_sync_log_entry *, void *);

/*
 * walk the log from one entry to another
 *
 * from: the start entry, cannot be NULL
 * to: the final entry, inclusive, can be NULL. if set to NULL, walk will stop 
 *      at the end of the last log page
 */
static nvpc_sync_log_entry *walk_log(nvpc_sync_log_entry *from, 
                            nvpc_sync_log_entry *to, 
                            log_walker it, void *opaque, bool write)
                            /* 
                             * According to our test, prefetch is faster for reads 
                             * and slower for writes on pmem. So we use prefetch 
                             * when write is false, and do nothing when write is true.
                             */
{
    nvpc_sync_log_entry *current_log = from;

    /* loop until current_log moves to the last page */
    while (((uintptr_t)current_log & PAGE_MASK) != 
            ((uintptr_t)to & PAGE_MASK))
    {
        /* loop in one page */
        while ((uintptr_t)current_log < 
                (uintptr_t)NVPC_LOG_ENTRY_NEXT(current_log))
        {
            // prefetch next cl if next entry is aligned
            if (!(((uintptr_t)(current_log+1))%L1_CACHE_BYTES))
            {
                (write) ? 
                    0 : 
                    prefetch(current_log+1);
            }
            // prefetch next page if next entry is the last
            if ((uintptr_t)(current_log+1) == (uintptr_t)NVPC_LOG_ENTRY_NEXT(current_log) && 
                NVPC_LOG_HAS_NEXT(current_log))
            {
                (write) ? 
                    0 : 
                    prefetch((void*)NVPC_LOG_NEXT(current_log));
            }

            if (it(current_log, opaque))
                return current_log;
            
            current_log++;
        }
        
        /* already at the last page */
        if(!NVPC_LOG_HAS_NEXT(current_log))
            goto out;
            
        /* move to the next page */
        current_log = (nvpc_sync_log_entry *)NVPC_LOG_NEXT(current_log);
    }
    
    /* walk the last page */
    while (current_log <= to)
    {
        if (!(((uintptr_t)current_log)%L1_CACHE_BYTES))
            (write) ? 
                0 : 
                prefetch(current_log+1);

        if (it(current_log, opaque))
            return current_log;

        current_log++;
    }
out:
    return current_log;
}

/* 
 * Append the inode log for n entries.
 * Returns the first free log entry that can be written to.
 * This can only happen when the data or metadata is updated on the 
 * inode. When this happen, the i_rwsem ensures that only one thread
 * is advancing this lock. So we don't need any lock here. 
 * NVTODO: make this available for concurrent access with atomic ops.
 */
static nvpc_sync_log_entry *append_inode_n_log(struct inode *inode, size_t n_entries)
{
    // NVXXX: prealloc many pages? or one by one?
    nvpc_sync_log_entry *ent = inode->nvpc_sync_ilog.log_tail;
    nvpc_sync_log_entry *ent_old = ent;
    int ents_left = NVPC_PAGE_LOG_ENTRIES_LEFT(ent);

    // walk & check if there are already enough log entries
    while (NVPC_LOG_HAS_NEXT(ent))
    {
        ent = (nvpc_sync_log_entry *)NVPC_LOG_NEXT(ent);
        ents_left += NVPC_PAGE_LOG_ENTRIES;
    }
    

    /* we need more log pages, prepare them here */
    if (n_entries > ents_left)
    {
        size_t need_pages = (n_entries - ents_left + NVPC_PAGE_LOG_ENTRIES - 1) / NVPC_PAGE_LOG_ENTRIES;
        void *last_page;
        int tail_new_offset;
        if (need_pages > 1) /* more than one page, should not happen */
        {
            struct list_head pages;
            struct page *page;
            void *kaddr;
            nvpc_next_log_entry *nextent;
            INIT_LIST_HEAD(&pages);
            if (create_n_new_log_pages(&pages, need_pages) < need_pages)
            {
                /* cannot alloc more pages for the log */
                return NULL;
            }
            nextent = NVPC_LOG_ENTRY_NEXT(ent);
            list_for_each_entry(page, &pages, lru) {
                kaddr = page_to_virt(page);
                nextent->next_log_page = nvpc_get_off_pg(kaddr);
                nextent->raw.type = NVPC_LOG_TYPE_NEXT;
                arch_wb_cache_pmem((void*)nextent, sizeof(nvpc_next_log_entry));
                nextent = NVPC_LOG_ENTRY_NEXT(kaddr);
            }
            memset(nextent, 0, sizeof(nvpc_next_log_entry));
            arch_wb_cache_pmem((void*)nextent, sizeof(nvpc_next_log_entry));
            last_page = kaddr;
        }
        else /* only one page needed */
        {
            void *newpage;
            nvpc_next_log_entry *nextent;
            newpage = create_new_log_page();
            if (!newpage)
                return NULL;
            
            nextent = NVPC_LOG_ENTRY_NEXT(ent);
            nextent->next_log_page = nvpc_get_off_pg(newpage);
            nextent->raw.type = NVPC_LOG_TYPE_NEXT;
            arch_wb_cache_pmem((void*)nextent, sizeof(nvpc_next_log_entry));
            last_page = newpage;
            nextent = NVPC_LOG_ENTRY_NEXT(newpage);
            memset(nextent, 0, sizeof(nvpc_next_log_entry));
            arch_wb_cache_pmem((void*)nextent, sizeof(nvpc_next_log_entry));
        }
        // nvpc_write_commit();
        tail_new_offset = (n_entries - ents_left) % NVPC_PAGE_LOG_ENTRIES;
        /* preset log_tail to preserve n entries */
        inode->nvpc_sync_ilog.log_tail = (nvpc_sync_log_entry*)last_page + tail_new_offset;
    }
    else
    {
        ent = ent_old;
        
        /* preset log_tail to preserve n entries */
        while (n_entries)
        {
            size_t this_page_ents = min_t(size_t, NVPC_PAGE_LOG_ENTRIES_LEFT(ent), n_entries);
            n_entries -= this_page_ents;
            ent += this_page_ents;
            if (n_entries)
                ent = (nvpc_sync_log_entry *)NVPC_LOG_NEXT(ent);
        }
        
        inode->nvpc_sync_ilog.log_tail = ent;
    }

    /* return old tail as the first log to write to */
    if (ent_old == (nvpc_sync_log_entry*)NVPC_LOG_ENTRY_NEXT(ent_old))
    {
        ent_old = (nvpc_sync_log_entry*)nvpc_get_addr_pg(((nvpc_next_log_entry*)ent_old)->next_log_page);
    }
    
    return ent_old;
}

/*
 * Move alongside the log for n entries
 */
// nvpc_sync_log_entry *advance_inode_n_log(nvpc_sync_log_entry *from, size_t n_entries)
// {

// }

/* returns 0 on success */
int write_oop(struct inode *inode, struct page *page, loff_t file_off, uint16_t len, uint32_t jid, 
        nvpc_sync_log_entry **new_head, nvpc_sync_log_entry **new_tail)
{
    nvpc_sync_write_entry *ent;
    WARN_ON(!PageNVPC(page));
    // WARN_ON(inode->i_sb->s_nvpc_flags & SB_NVPC_ON);

    ent = (nvpc_sync_write_entry *)append_inode_n_log(inode, 1);
    if (!ent)
        return -ENOMEM;
    
    ent->raw.type = NVPC_LOG_TYPE_WRITE;
    ent->raw.flags = 0;
    ent->raw.id = inode->nvpc_sync_ilog.log_cntr++;
    ent->file_offset = file_off;
    ent->data_len = len;
    ent->page_index = nvpc_get_off_pg(page_to_virt(page));
    ent->jid = jid;
    arch_wb_cache_pmem((void*)ent, sizeof(nvpc_sync_write_entry));
    
    /* commit later in write_commit() */
    
    if (new_head) *new_head = (nvpc_sync_log_entry*)ent;
    /* warn: needs atomic under concurrency */
    if (new_tail) *new_tail = inode->nvpc_sync_ilog.log_tail;

    return 0;
}

// NVTODO: try to seperate copy and flush, use 2 walks
static bool __write_ioviter_to_log_wkr(nvpc_sync_log_entry *ent, void *opaque)
{
    struct iov_iter *from = (struct iov_iter *)opaque;
    _copy_from_iter_flushcache(ent, NVPC_LOG_ENTRY_SIZE, from);
    // WARN_ON(wrsz > NVPC_LOG_ENTRY_SIZE);
    return false;
}

/* returns 0 on success */
int write_ip(struct inode *inode, struct iov_iter *from, loff_t file_off, uint32_t jid, 
        nvpc_sync_log_entry **new_head, nvpc_sync_log_entry **new_tail)
{
    size_t len = iov_iter_count(from);
    size_t nentries = (len + NVPC_LOG_ENTRY_SIZE - 1) / NVPC_LOG_ENTRY_SIZE + 1;
    nvpc_sync_write_entry *ent0;
    nvpc_sync_log_entry *last;
    // WARN_ON(inode->i_sb->s_nvpc_flags & SB_NVPC_ON);
    ent0 = (nvpc_sync_write_entry *)append_inode_n_log(inode, nentries);
    if (!ent0)
        return -ENOMEM;
    
    last = walk_log((nvpc_sync_log_entry *)(ent0+1), inode->nvpc_sync_ilog.log_tail-1, 
                __write_ioviter_to_log_wkr, from, true);
    // WARN_ON(last != inode->nvpc_sync_ilog.log_tail);
    
    ent0->raw.id = inode->nvpc_sync_ilog.log_cntr++;
    ent0->raw.type = NVPC_LOG_TYPE_WRITE;
    ent0->raw.flags = 0;
    ent0->file_offset = file_off;
    ent0->data_len = len;
    ent0->page_index = 0;
    ent0->jid = jid;
    arch_wb_cache_pmem((void*)ent0, sizeof(nvpc_sync_write_entry));

    /* commit later in write_commit() */

    if (new_head) *new_head = (nvpc_sync_log_entry*)ent0;
    /* warn: needs atomic under concurrency */
    if (new_tail) *new_tail = inode->nvpc_sync_ilog.log_tail;

    return 0;
}

void write_commit(struct inode *inode, nvpc_sync_log_entry *old_tail, nvpc_sync_log_entry *tail)
{
    /* spin here, wait for the last log to finish, useless now */
    /* needs to be atomic if we have concurrency here */
    // while (inode->nvpc_sync_ilog.log_head->committed_log_tail != old_tail)
    // {
    //     ;
    // }
    if (inode->nvpc_sync_ilog.log_head->committed_log_tail != old_tail)
    {
        WARN(1, "[NVPC BUG]: log tail not match! expected_tail: %px, committed_tail: %px\n", old_tail, inode->nvpc_sync_ilog.log_head->committed_log_tail);
    }
    
    
    inode->nvpc_sync_ilog.log_head->committed_log_tail = tail;
    arch_wb_cache_pmem(
        (void*)&inode->nvpc_sync_ilog.log_head->committed_log_tail, 
        sizeof(nvpc_sync_write_entry*)
    );
    // NVTODO: when to remove I_NVPC_DATA? maybe the cleanup thread
    // spin_lock(&inode->i_lock);
    inode->nvpc_i_state |= I_NVPC_DATA;
    // spin_unlock(&inode->i_lock);
    
    nvpc_write_commit();
}

struct nvpc_sync_transaction_part
{
    nvpc_sync_write_entry* ent; // start entry of this trans
    struct page *page;          // oop page, NULL for ip
    nvpc_sync_write_entry* _oldent; 
    struct page *_oldpage;
    bool free_n_mark;
    bool rollback;
    loff_t file_off;
    nvpc_sync_page_info_t *info;
    // struct xa_state xas;
};

struct nvpc_sync_transaction
{
    int count;
    struct inode *inode;
    nvpc_sync_log_entry *from;
    nvpc_sync_log_entry *to;
};


static void wb_log_work_fn(struct work_struct *work);
static inline struct nvpc_sync_page_info *new_info_struct(struct inode *inode)
{
    nvpc_sync_page_info_t *info;
    info = kzalloc(sizeof(nvpc_sync_page_info_t), GFP_ATOMIC);
    if (!info)
        return NULL;
    INIT_WORK(&info->wb_log_work, wb_log_work_fn);
    spin_lock_init(&info->infolock);
    info->inode = inode;
    return info;
}


// do write log and update tail respectively, to support transaction

int nvpc_fsync_range(struct file *file, loff_t start, loff_t end, int datasync)
{
    // get the pages from start to end, decide to perform write ip or oop.
    // refer to generic_perform_write(), ext4_write_begin() and grab_cache_page_write_begin()
    
    size_t bytes_left;
    size_t written = 0;
    loff_t pos = start;
    struct inode *inode = file->f_inode;
    struct address_space *mapping = file->f_mapping;
    struct nvpc_sync_transaction trans;
    nvpc_sync_log_entry *old_tail;
    nvpc_sync_log_entry *new_tail;
    int ret = 0;
    bool fail = false;
    uint32_t old_id;
    loff_t fsize;
    // bool drained = false;
    struct pagevec pvec;
    int nr_pages;
    pgoff_t index_off;
    pgoff_t end_off;
    
    fsize = i_size_read(inode);
    if (end > fsize)    // truncate end to fsize
    {
        pr_debug("[NVPC DEBUG]: nvpc_fsync_range cut the end here from %lld to %lld\n", 
            end, fsize);
        end = fsize;
    }
    
    bytes_left = end - start + 1;

    pr_debug("[NVPC DEBUG]: nvpc_fsync_range @i %lu start %lld end %lld ds %d\n", inode->i_ino, start, end, datasync);

    nvpc_get_log_inode(inode);

    // this can be a very big lock ... refer to nvpc_sync.h for a fine grained solution
    mutex_lock(&inode->nvpc_sync_ilog.log_lock);

    old_id = inode->nvpc_sync_ilog.log_cntr;

    /* NVNEXT: not safe under concurrency, use the old_tail value from the first write */
    old_tail = new_tail = inode->nvpc_sync_ilog.log_tail;

    pr_debug("[NVPC DEBUG]: nvpc_fsync_range 0\n");

    trans.count = 0;
    trans.inode = inode;

    pr_debug("[NVPC DEBUG]: nvpc_fsync_range 1\n");

    pagevec_init(&pvec);
    index_off = start >> PAGE_SHIFT;
    end_off = end >> PAGE_SHIFT;

    while (index_off <= end_off && index_off != (pgoff_t)-1)
    {
        unsigned pv_i;
        int fallback = 0;
        nr_pages = pagevec_lookup_range_tag(&pvec, mapping, &index_off,
				end_off, PAGECACHE_TAG_TOWRITE);    
                // we should use actually PAGECACHE_TAG_DIRTY, but it is toooo slow... 
                // it is ok if we have more TOWRITE for writeback work, and it is also
                // ok if TOWRITE is cleared by some writeback work. 
        if (!nr_pages)
			break;
        
        for (pv_i = 0; pv_i < nr_pages; pv_i++)
        {
            struct page *page = pvec.pages[pv_i];
            pgoff_t index = page->index;
            loff_t offset;
            loff_t offtail;
            size_t bytes;
            unsigned long irqflags;
            // struct page *nv_pg = NULL;
            nvpc_sync_write_entry *newent;
            
            /* page should be in the page cache. but maybe it's just evicted? */
            // int fgp_flags = FGP_LOCK;

            if ((start >> PAGE_SHIFT) > index || (end >> PAGE_SHIFT) < index)
            {
                /* maybe we are fetching a page that writeback is using, which is out of our expected range */
                continue;
            }
            
            if (((unsigned long long)index<<PAGE_SHIFT) <= start)
            {
                WARN_ON(index != (start >> PAGE_SHIFT));
                pos = start;
            }
            else
            {
                pos = (unsigned long long)index<<PAGE_SHIFT;
            }
            if ((end >> PAGE_SHIFT) == index)
            {
                offtail = (end+1) % PAGE_SIZE;  // exclusive tail
            }
            
            offset = (pos & (PAGE_SIZE - 1));
            bytes = offtail - offset; // min_t(unsigned long, PAGE_SIZE - offset, bytes_left);
            

            // page = pagecache_get_page(mapping, index, fgp_flags, mapping_gfp_mask(mapping));
            /* don't need wait_for_stable_page(), because we don't write to the page */

            // pr_info("[NVPC DEBUUG]: nvpc_fsync_range idx %lu nextoff %lu\n", index, index_off);

            pr_debug("[NVPC DEBUG]: nvpc_fsync_range 2\n");

            // WARN_ON(!page);
            if (!page)
            {
                pr_debug("[NVPC DEBUG]: nvpc_fsync_range (!page)\n");
                /* -ENOMEM */
                // fail = true;
                // drained = true;
                goto out1;
            }

            /* only persist dirty non-persisted pages in npvc; pre-check to avoid lock */
            if (!PageNVPCNpDirty(page))
            {
                pr_debug("[NVPC DEBUG]: nvpc_fsync_range (!PageNVPCNpDirty(page))\n");
                pr_debug("[NVPC DEBUG]: nvpc_fsync_range dirty? %d\n", PageDirty(page));
                goto out1;
            }

            lock_page(page);

            /* only persist dirty non-persisted pages in npvc */
            if (!PageNVPCNpDirty(page))
            {
                pr_debug("[NVPC DEBUG]: nvpc_fsync_range (!PageNVPCNpDirty(page))\n");
                pr_debug("[NVPC DEBUG]: nvpc_fsync_range dirty? %d\n", PageDirty(page));
                goto out;
            }
            
            /* write it to the log */
            pr_debug("[NVPC DEBUG]: nvpc_fsync_range 3\n");

            // struct page *nv_pg = NULL;
            // nvpc_sync_write_entry *newent;
            pr_debug("[NVPC DEBUG]: nvpc_fsync_range 6\n");


            /* bytes is less than ip threshold, and there is previous oop write */
            // NVTODO: ip when all open fd are sync
            if (bytes < NVPC_IPOOP_THR && PageNVPCPin(page))
            {
                struct iov_iter i;
                struct kvec kvec;
                kvec.iov_base = page_to_virt(page) + offset;
                kvec.iov_len = bytes;
                iov_iter_kvec(&i, READ, &kvec, 1, bytes);
                pr_debug("[NVPC DEBUG]: ip @1\n");
                if (write_ip(inode, &i, pos, old_id, 
                    (nvpc_sync_log_entry**)&newent, &new_tail))
                {
                    fail = true;
                    // drained = true;
                    goto out;
                }
            }
            /* large write, or no previous write, oop */
            else
            {
                struct page *log_pg;
                
                /* strict mode or relax mode */
                log_pg = nvpc_get_new_page(NULL, 0);
                if (!log_pg)
                {
                    fail = true;
                    // drained = true;
                    goto out;
                }
                copy_highpage(log_pg, page);
                arch_wb_cache_pmem(page_to_virt(log_pg), PAGE_SIZE);
                pr_debug("[NVPC DEBUG]: oop @2\n");
                // oop will add log_pg to nvpc index, migration later can use this info
                if (write_oop(inode, log_pg, pos, (uint16_t)bytes, old_id, 
                    (nvpc_sync_log_entry**)&newent, &new_tail))
                {
                    fail = true;
                    // drained = true;
                    goto out;
                }
            }
            
            

            pr_debug("[NVPC DEBUG]: nvpc_fsync_range 7\n");

            /* 
            * DO NOT clear PageNVPCNpDirty anywhere else, the data on disk may
            * be newer than nvpc and thus cause inconsistency on recovery.
            */
            ClearPageNVPCNpDirty(page);
            xa_lock_irqsave(&mapping->i_pages, irqflags);
            __xa_clear_mark(&mapping->i_pages, page->index, PAGECACHE_TAG_TOWRITE);
            xa_unlock_irqrestore(&page->mapping->i_pages, irqflags);
            
            // PageNVPCPDirty is cleared on writeback finish
            SetPageNVPCPin(page);
            SetPageNVPCPDirty(page);
            
out:
            pr_debug("[NVPC DEBUG]: nvpc_fsync_range 8 out\n");
            if (unlikely(!page))    // useless
            {
                fallback = true;
                goto fallback;
            }

            unlock_page(page);
            // put_page(page);

            if (fail)
            {
                fallback = true;
                goto fallback;
            }

            written += bytes;
out1:
            bytes_left -= bytes;
            // pos += bytes;
            trans.count++;

        }
fallback:
        pagevec_release(&pvec);
        if (fallback)
            goto fallback1;
        cond_resched();
    }

    pr_debug("[NVPC DEBUG]: nvpc_fsync_range 9\n");

    // first commit the persistent log tail
    write_commit(inode, old_tail, new_tail);

    trans.from = old_tail;
    trans.to = NULL;

    mutex_unlock(&inode->nvpc_sync_ilog.log_lock);

    pr_debug("[NVPC DEBUG]: nvpc_fsync_range 10\n");

    // write_commit(inode, old_tail, new_tail);
    return ret;

fallback1:
    pr_debug("[NVPC DEBUG]: meet fallback under fsync\n");
    inode->nvpc_sync_ilog.log_tail = old_tail;
    inode->nvpc_sync_ilog.log_cntr = old_id;
    mutex_unlock(&inode->nvpc_sync_ilog.log_lock);

    // if (drained)
    // {
    //     try_to_free_pages
    // }

    ret = file->f_op->fsync(file, start, end, datasync);
    return ret;
}

struct nvpc_copy_pending_s {
    // struct page *origin;    // origin pagecache page
    struct page *nvpg;      // copied nvpc oop page
    struct inode *inode;    // origin inode
    pgoff_t index;          // origin index
    struct work_struct work;
};

// static void nvpc_copy_pending_page_fn(struct work_struct *work)
// {
//     struct page *origin;
//     // struct inode *inode;
//     struct nvpc_copy_pending_s *pending;
//     nvpc_sync_log_entry *old_tail, *new_tail, *ent;
//     bool free_pg = false;

//     pr_debug("[NVPC DEBUG]: copy_pending_page work 0\n");

//     pending = container_of(work, struct nvpc_copy_pending_s, work);

//     mutex_lock(&pending->inode->nvpc_sync_ilog.log_lock);
//     old_tail = new_tail = pending->inode->nvpc_sync_ilog.log_tail;

//     origin = pagecache_get_page(pending->inode->i_mapping, pending->index, 
//         FGP_LOCK, mapping_gfp_mask(pending->inode->i_mapping));

//     WARN_ON(!origin);
//     if (!origin)
//         goto free;

//     // NVTODO: if we don't need to log this page
//     if ()
//     {
//         free_pg = true;
//         goto unlock;
//     }

//     // copy_highpage(log_pg, page);
//     arch_wb_cache_pmem(page_to_virt(pending->nvpg), PAGE_SIZE);

//     if (write_oop(pending->inode, pending->nvpg, pending->index << PAGE_SHIFT, 
//         PAGE_SIZE, &ent, &new_tail))
//     {
//         free_pg = true;
//         goto unlock;
//     }

//     // NVTODO: add log_pg to nvpc index, commit
    

// unlock:
//     unlock_page(origin);
//     put_page(origin);
// free:
//     mutex_unlock(&pending->inode->nvpc_sync_ilog.log_lock);
//     if (free_pg)
//     {
//         pr_debug("[NVPC DEBUG]: copy_pending_page work free\n");
//         nvpc_free_page(pending->nvpg, 0);
//     }
//     kfree(pending);
//     pr_debug("[NVPC DEBUG]: copy_pending_page work ok\n");
// }

// DEPRECATED
// int nvpc_copy_pending_page(struct page *page) 
// {
//     struct page *log_pg;
//     struct inode *inode;
//     // nvpc_sync_log_entry *old_tail, *new_tail;
//     nvpc_sync_page_info_t *prev_ent_info;
//     // struct nvpc_copy_pending_s *pending;
//     // int ret;

//     WARN_ON(!PageLocked(page));
//     pr_debug("[NVPC DEBUG]: copy_pending_page 0\n");
    
//     inode = page->mapping->host;

//     log_pg = nvpc_get_new_page(NULL, 0);
//     if (!log_pg)
//         goto err;
    
//     // issue a new work
//     // pending = kzalloc(sizeof(struct nvpc_copy_pending_s), GFP_ATOMIC);
//     // if (!pending)
//     //     goto err1;
    
//     copy_highpage(log_pg, page);

//     // pending->index = page->index;
//     // pending->inode = inode;
//     // pending->nvpg = log_pg;
//     // INIT_WORK(&pending->work, nvpc_copy_pending_page_fn);
//     // ret = queue_work(nvpc_sync.nvpc_sync_wq, &pending->work);

//     // find the entry and rewrite it directly, meanwhile nobody may race with us so we don't need lock
    
//     prev_ent_info = xa_load(&inode->nvpc_sync_ilog.inode_log_pages, page->index);
//     WARN_ON(!prev_ent_info);
//     if (unlikely(!prev_ent_info))
//         goto err1;
    
//     mutex_lock(&inode->nvpc_sync_ilog.compact_lock);
//     // the latest write should always be oop
//     WARN_ON(prev_ent_info->latest_write->page_index == 0);
//     prev_ent_info->latest_write->page_index = nvpc_get_off_pg(page_to_virt(page));
//     arch_wb_cache_pmem(prev_ent_info->latest_write, sizeof(nvpc_sync_write_entry*));
//     mutex_unlock(&inode->nvpc_sync_ilog.compact_lock);

//     return 0;
// err1:
//     nvpc_free_page(log_pg, 0);
// err:
//     return -ENOMEM;
// }

/* 
 * NVPC truncate log represents that the data before this point inside NVPC
 * is truncated. This truncate operation will be replayed on the inode anyway,  
 * even without being written back or synced before a power fail. 
 */
int nvpc_sync_setattr(struct user_namespace *mnt_userns, struct dentry *dentry,
		   struct iattr *iattr)
{
    nvpc_sync_attr_entry *ent;
    nvpc_sync_log_entry *old_tail, *new_tail;

    // only deal with truncate. ignore time modify
    if (!(iattr->ia_valid & ATTR_SIZE))
        return 0;

    nvpc_get_log_inode(dentry->d_inode);
    pr_debug("[NVPC DEBUG]: truncate to %lld\n", iattr->ia_size);

    mutex_lock(&dentry->d_inode->nvpc_sync_ilog.log_lock);
    old_tail = dentry->d_inode->nvpc_sync_ilog.log_tail;
    ent = (nvpc_sync_attr_entry *)append_inode_n_log(dentry->d_inode, 1);
    if (!ent)
        return -ENOMEM;
    
    ent->raw.type = NVPC_LOG_TYPE_ATTR;
    ent->raw.flags = 0;
    ent->raw.id = dentry->d_inode->nvpc_sync_ilog.log_cntr++;
    ent->new_size = iattr->ia_size;
    ent->last_attr = dentry->d_inode->nvpc_sync_ilog.latest_logged_attr ? 
        nvpc_get_off(dentry->d_inode->nvpc_sync_ilog.latest_logged_attr) :
        0;
    arch_wb_cache_pmem((void*)ent, sizeof(nvpc_sync_attr_entry));

    new_tail = dentry->d_inode->nvpc_sync_ilog.log_tail;
    write_commit(dentry->d_inode, old_tail, new_tail);
    dentry->d_inode->nvpc_sync_ilog.latest_logged_attr = ent;
    mutex_unlock(&dentry->d_inode->nvpc_sync_ilog.log_lock);

    return 0;
}

struct walk_state {
    char *s;
    char *curr;
    size_t len;
    size_t left;
};

static bool __print_log_wkr(nvpc_sync_log_entry *ent, void *opaque)
{
    nvpc_sync_write_entry *went = (nvpc_sync_write_entry *)ent;
    int i = 0;
    struct walk_state *ws = opaque;
    
    if (!ws->s)
    {
        pr_info("[NVPC DEBUG] ent %d: pgidx %u off %lld len %u \n", i, went->page_index, went->file_offset, went->data_len);
        if (went->page_index)
        {
            // oop
            ws->s = kmalloc(went->data_len+1, GFP_KERNEL);
            ws->s[went->data_len] = 0;
            memcpy(ws->s, nvpc_get_addr_pg(went->page_index)+(went->file_offset&(~PAGE_MASK)), went->data_len);
            pr_info("[NVPC DEBUG] %s\n", ws->s);
            kfree(ws->s);
            ws->s = NULL;
            ws->curr = NULL;
        }
        else
        {
            // ip, head
            ws->s = kmalloc(went->data_len+1, GFP_KERNEL);
            ws->s[went->data_len] = 0;
            ws->len = went->data_len;
            ws->left = went->data_len;
            ws->curr = ws->s;
        }
        
    }
    else
    {
        // ip, body
        size_t bytes = min_t(unsigned long, NVPC_LOG_ENTRY_SIZE, ws->left);
        memcpy(ws->curr, ent, NVPC_LOG_ENTRY_SIZE);
        ws->left -= bytes;
        ws->curr += bytes;
        if (ws->left == 0)
        {
            // ip, tail
            pr_info("[NVPC DEBUG] %s\n", ws->s);
            kfree(ws->s);
            ws->s = NULL;
            ws->curr = NULL;
        }
    }
    
    return false;
}

void nvpc_print_inode_log(struct inode *inode)
{
    // nvpc_sync_log_entry *h = (nvpc_sync_log_entry *)inode->nvpc_sync_ilog.log_head->head_log_page;
    nvpc_sync_log_entry *h = (nvpc_sync_log_entry *)nvpc_get_addr_pg(nvpc_get_log_inode(inode)->head_log_page);
    struct walk_state ws = {
        .s = NULL, 
        .curr = NULL, 
        .left = 0, 
        .len = 0,
    };
    walk_log(h, inode->nvpc_sync_ilog.log_head->committed_log_tail, __print_log_wkr, &ws, false);
}

// use a bitmap to track if an ip write entry contributes to this page's newest content
typedef struct page_bytes_tracker_s {
    uint8_t bitmap[PAGE_SIZE / 8];
}page_bytes_tracker;

#define INIT_TRACKER(tracker) memset((tracker)->bitmap, 0, PAGE_SIZE / 8)

// return if current record is useful
bool nvpc_track_page_get_avail(page_bytes_tracker *tracker, int start, int len)
{
    int idx = start / 8;
    int off = start % 8;
    int curr_left;
    uint8_t curr_mask;

    bool avail = false;

    while (len && idx < PAGE_SIZE / 8)
    {
        curr_left = min_t(int, 8-off, len);
        curr_mask = ((1ul<<curr_left)-1) << off;
        avail = avail || ((~(tracker->bitmap[idx])) & curr_mask);
        tracker->bitmap[idx] |= curr_mask;
        len -= curr_left;
        idx++;
        off=0;
    }
    return avail;
}

static bool __copy_from_ip_log_wkr(nvpc_sync_log_entry *ent, void *opaque)
{
    struct iov_iter *to = (struct iov_iter *)opaque;
    size_t ret;
    ret = copy_to_iter(ent, NVPC_LOG_ENTRY_SIZE, to);

    // return true if there's nothing left
    return !iov_iter_count(to);
}

/* move the attr pointer to the next attr after ent, return if the required attr is found */
static inline bool __nvpc_find_relevant_attr(nvpc_sync_write_entry *ent, 
        nvpc_sync_attr_entry **attr)
{
    nvpc_sync_attr_entry *a1, *a2;
    a1 = *attr;
    a2 = NULL;
    
    // find the very next attr after current entry
    while (a1 && a1->raw.id > ent->raw.id)
    {
        a2 = a1;
        a1 = a1->last_attr ? (nvpc_sync_attr_entry *)nvpc_get_addr(a1->last_attr) : NULL;
    }

    if (!a2)
        return false;
    
    *attr = a2;
    return true;
}

// return a new page in DRAM, caller needs to free it
struct page *nvpc_get_page_from_entry(nvpc_sync_write_entry *ent, nvpc_sync_attr_entry *latest_attr, void *data_pg, char *mask_pg)
{
    nvpc_sync_attr_entry *curr_attr;
    struct page *newpg;
    void *newpg_addr;
    BUG_ON(!ent);
    BUG_ON(ent->raw.flags & NVPC_LOG_FLAG_WRFRE);

    newpg_addr = data_pg; // (void*)get_zeroed_page(GFP_KERNEL);
    newpg = virt_to_page(newpg_addr);
    if (!newpg_addr)
        return NULL;
    curr_attr = latest_attr;

    if (ent->page_index)
    {
        bool attr_found = false;
        // oop
        pr_debug("[NVPC DEBUG] oop off %llu len %llu\n", ent->file_offset, (uint64_t)ent->data_len);
        
        if (curr_attr)
        {
            attr_found = __nvpc_find_relevant_attr(ent, &curr_attr);
            pr_debug("[NVPC DEBUG] curr_attr %px found %d sz %llu\n", curr_attr, attr_found, curr_attr->new_size);
        }

        copy_highpage(newpg, virt_to_page(nvpc_get_addr_pg(ent->page_index)));
        memset(mask_pg, 1, PAGE_SIZE);
        if (attr_found)
        {
            if ((curr_attr->new_size & PAGE_MASK) == (ent->file_offset & PAGE_MASK))
            {
                memset(newpg_addr+(curr_attr->new_size & (~PAGE_MASK)), 0, PAGE_SIZE - (curr_attr->new_size & (~PAGE_MASK)));
                memset(mask_pg+(curr_attr->new_size & (~PAGE_MASK)), 0, PAGE_SIZE - (curr_attr->new_size & (~PAGE_MASK)));
            }
            else if ((curr_attr->new_size & PAGE_MASK) < (ent->file_offset & PAGE_MASK))
            {
                // free_page((unsigned long)newpg_addr);
                // return NULL;
                memset(newpg_addr, 0, PAGE_SIZE);
                memset(mask_pg, 0, PAGE_SIZE);
            }
        }
    }
    else
    {
        bool attr_found = false;
        bool first_attr = true; // only first attr contributes to mask_pg
        // ip
        nvpc_sync_write_entry *curr = ent;
        page_bytes_tracker *tracker;
        // NVXXX: this is too huge (takes 8 pages)
        nvpc_sync_write_entry **valid_list;
        int valid_i = 0;

        tracker = kmalloc(sizeof(page_bytes_tracker), GFP_KERNEL);
        if (!tracker)
        {
            // free_page((unsigned long)newpg_addr);
            return NULL;
        }
        valid_list = kmalloc_array(PAGE_SIZE, sizeof(nvpc_sync_write_entry *), GFP_KERNEL);
        if (!valid_list)
        {
            // free_page((unsigned long)newpg_addr);
            kfree(tracker);
            return NULL;
        }

        INIT_TRACKER(tracker);
        
        // go back and track
        pr_debug("[NVPC DEBUG] ip track\n");

        memset(mask_pg, 1, PAGE_SIZE);
        
        // loop while current is ip
        while (!curr->page_index)
        {
            if (curr->raw.flags & NVPC_LOG_FLAG_WREXP)
                goto apply;
            // track and check if this entry is useful
            if (curr_attr)
                attr_found = __nvpc_find_relevant_attr(curr, &curr_attr);
            // if curr attr is truncating the curr ent page, set all mask bits after the truncate to 1
            if (attr_found)
            {
                pr_debug("[NVPC DEBUG]: next_trunc: %lld\n", curr_attr->new_size);
                if ((curr_attr->new_size&PAGE_MASK) == (curr->file_offset&PAGE_MASK))
                    nvpc_track_page_get_avail(tracker, (curr_attr->new_size)&(~PAGE_MASK), PAGE_SIZE / 8);
                else if ((curr_attr->new_size&PAGE_MASK) < (curr->file_offset&PAGE_MASK))
                    nvpc_track_page_get_avail(tracker, 0, PAGE_SIZE / 8);
                if (first_attr)
                {
                    first_attr = false;
                    if ((curr_attr->new_size & PAGE_MASK) == (curr->file_offset & PAGE_MASK))
                        memset(mask_pg+(curr_attr->new_size & (~PAGE_MASK)), 0, PAGE_SIZE - (curr_attr->new_size & (~PAGE_MASK)));
                    else if ((curr_attr->new_size & PAGE_MASK) < (curr->file_offset & PAGE_MASK))
                        memset(mask_pg, 0, PAGE_SIZE);
                }
                
            }

            if (nvpc_track_page_get_avail(tracker, (curr->file_offset)&(~PAGE_MASK), curr->data_len))
            {
                valid_list[valid_i] = curr;
                valid_i++;
            }

            pr_debug("[NVPC DEBUG] ip valid_i: %d off %llu len %llu\n", valid_i, curr->file_offset, (uint64_t)curr->data_len);

            // NVTODO: check if last_write is still available! not necessary, we are keeping the latest write ent now
            // current ip has previous write link
            if (curr->last_write) 
                curr = (nvpc_sync_write_entry *)nvpc_get_addr(curr->last_write);
            // done if this is the first ip
            else
            {
                curr = NULL;
                break;
            }
        }

        // if there's a whole page oop left
        if (curr)
        {
            // bool attr_found = false;
            if (curr_attr)
                attr_found = __nvpc_find_relevant_attr(curr, &curr_attr);

            pr_debug("[NVPC DEBUG] oop off %llu len %llu\n", curr->file_offset, (uint64_t)curr->data_len);
                
            copy_highpage(newpg, virt_to_page(nvpc_get_addr_pg(curr->page_index)));

            if (attr_found)
            {
                pr_debug("[NVPC DEBUG]: next_trunc: %lld\n", curr_attr->new_size);
                if ((curr_attr->new_size & PAGE_MASK) == (curr->file_offset & PAGE_MASK))
                    memset(newpg_addr+(curr_attr->new_size & (~PAGE_MASK)), 0, PAGE_SIZE - (curr_attr->new_size & (~PAGE_MASK)));
                else if ((curr_attr->new_size & PAGE_MASK) < (curr->file_offset & PAGE_MASK))
                    memset(newpg_addr, 0, PAGE_SIZE);
                if (first_attr)
                {
                    first_attr = false;
                    if ((curr_attr->new_size & PAGE_MASK) == (curr->file_offset & PAGE_MASK))
                        memset(mask_pg+(curr_attr->new_size & (~PAGE_MASK)), 0, PAGE_SIZE - (curr_attr->new_size & (~PAGE_MASK)));
                    else if ((curr_attr->new_size & PAGE_MASK) < (curr->file_offset & PAGE_MASK))
                        memset(mask_pg, 0, PAGE_SIZE);
                }
            }
        }
apply:
        for (; valid_i > 0; valid_i--)
        {
            // walk ip and copy to new page
            struct iov_iter i;
            struct kvec kvec;
            curr = valid_list[valid_i-1];
            kvec.iov_base = newpg_addr + ((curr->file_offset)&(~PAGE_MASK));
            kvec.iov_len = curr->data_len;
            iov_iter_kvec(&i, WRITE, &kvec, 1, curr->data_len);
            walk_log((nvpc_sync_log_entry *)curr+1, NULL, __copy_from_ip_log_wkr, &i, false);
        }

        kfree(tracker);
        kfree(valid_list);
    }
    
    return newpg;
}

// DEPRECATED
// void nvpc_print_inode_pages(struct inode *inode)
// {
//     pgoff_t i;
//     nvpc_sync_page_info_t *ent_info;
//     xa_for_each(&inode->nvpc_sync_ilog.inode_log_pages, i, ent_info) 
//     {
//         struct page *page;
//         nvpc_sync_write_entry *ent = ent_info->latest_write;
//         void *newpg_addr = (void*)get_zeroed_page(GFP_KERNEL);
//         if (!newpg_addr)
//             continue;
//         page = nvpc_get_page_from_entry(
//             (nvpc_sync_write_entry*)ent, 
//             (nvpc_sync_attr_entry*)inode->nvpc_sync_ilog.latest_logged_attr, newpg_addr, NULL);

//         pr_debug("[NVPC DEBUG]: raw %s\n[NVPC DEBUG]: ------------\n", (char*)page_to_virt(page));
//         pr_debug("[NVPC DEBUG]: sz %lld wb %d\n", inode->i_size, ent->raw.flags & NVPC_LOG_FLAG_WREXP ? 1 : 0);
        
//         // cut the page according to the current file size
//         pr_debug("[NVPC DEBUG]: cut info i %lld e %lld ir %lld 1st %d 4th %d 5th %d lst %d, pg %px cutpt %px\n", 
//             (inode->i_size & PAGE_MASK), 
//             (((nvpc_sync_write_entry*)ent)->file_offset & PAGE_MASK), 
//             (inode->i_size & (~PAGE_MASK)), 
//             ((char*)page_to_virt(page))[0], 
//             ((char*)page_to_virt(page))[3], 
//             ((char*)page_to_virt(page))[4], 
//             ((char*)page_to_virt(page))[(inode->i_size & (~PAGE_MASK))-2], 
//             page_to_virt(page),
//             page_to_virt(page)+(inode->i_size & (~PAGE_MASK)));
//         if ((inode->i_size & PAGE_MASK) == (((nvpc_sync_write_entry*)ent)->file_offset & PAGE_MASK))
//             memset(page_to_virt(page)+(inode->i_size & (~PAGE_MASK)), 0, PAGE_SIZE - (inode->i_size & (~PAGE_MASK)));
//         else if ((inode->i_size & PAGE_MASK) < (((nvpc_sync_write_entry*)ent)->file_offset & PAGE_MASK))
//             memset(page_to_virt(page), 0, PAGE_SIZE);
        
//         pr_info("[NVPC DEBUG]: fileoff %lu 1st %c\n", i, ((char*)page_to_virt(page))[0]);
//         pr_info("[NVPC DEBUG]: %s\n[NVPC DEBUG]: ------------\n", (char*)page_to_virt(page));

//         free_page((unsigned long)page_to_virt(page));
//     }
// }

// record the last write entry before writeback is set, the page should be locked
void nvpc_mark_page_writeback(struct page *page) 
{
    struct inode *inode = page->mapping->host;
    unsigned long flags;

    nvpc_sync_page_info_t *prev_ent_info, *ret;
    
    prev_ent_info = xa_load(&inode->nvpc_sync_ilog.inode_log_pages, page->index);
    if (!prev_ent_info)
    {
        prev_ent_info = new_info_struct(inode);
        prev_ent_info->idx = page->index;
        xa_lock_irqsave(&inode->nvpc_sync_ilog.inode_log_pages, flags);
        ret = __xa_store(&inode->nvpc_sync_ilog.inode_log_pages, page->index, prev_ent_info, GFP_NOWAIT);
        xa_unlock_irqrestore(&inode->nvpc_sync_ilog.inode_log_pages, flags);
        if (xa_err(ret))
            return; // may need to do something here
    }

    // this assignment is atomic because the page has been locked
    // lock the info to prevent racing with info update in sync commit
    spin_lock_irqsave(&prev_ent_info->infolock, flags);
    prev_ent_info->jid = inode->nvpc_sync_ilog.log_cntr;
    spin_unlock_irqrestore(&prev_ent_info->infolock, flags);
    pr_debug("[NVPC DEBUG]: wb set end\n");
}

// queue log work when writeback is cleared, the page should be locked
void nvpc_log_page_writeback(struct page *page)
{
    struct inode *inode = page->mapping->host;
    int ret;
    // bool should_work = false;
    unsigned long flags;

    nvpc_sync_page_info_t *prev_ent_info;
    prev_ent_info = xa_load(&inode->nvpc_sync_ilog.inode_log_pages, page->index);
    if (!prev_ent_info)
        return;

    WARN_ON(!prev_ent_info);
    spin_lock_irqsave(&prev_ent_info->infolock, flags);
    prev_ent_info->committed_jid = prev_ent_info->jid;
    prev_ent_info->pagecache_page = page;
    spin_unlock_irqrestore(&prev_ent_info->infolock, flags);
    pr_debug("[NVPC DEBUG]: wb clear unlock\n");
    
    // use work queue to postpone the logging
    ret = queue_work(nvpc_sync.nvpc_sync_wq, &prev_ent_info->wb_log_work);
    pr_debug("[NVPC DEBUG]: wb clear end\n");
}

// log writeback and cancel previously marked entry, clear PageNVPCPDirty
static void wb_log_work_fn(struct work_struct *work)
{
    nvpc_sync_page_info_t *info;
    nvpc_sync_log_entry *old_tail, *new_tail;
    nvpc_sync_wb_entry *ent;
    struct inode *inode;
    struct page *pagecache_page;
    // bool wb_clear_pin;
    unsigned long flags;
    uint64_t jid;
    pgoff_t index;

    pr_debug("[NVPC DEBUG]: wb work fn\n");
    info = container_of(work, nvpc_sync_page_info_t, wb_log_work);
    inode = info->inode;
    if (!atomic_read(&inode->i_count))
        return;

    // extract info out and release the lock
    // spin_lock_bh(&info->infolock);
    spin_lock_irqsave(&info->infolock, flags);
    jid = info->committed_jid;
    pagecache_page = info->pagecache_page;
    index = info->idx;
    spin_unlock_irqrestore(&info->infolock, flags);


    mutex_lock(&inode->nvpc_sync_ilog.log_lock);
    old_tail = new_tail = inode->nvpc_sync_ilog.log_tail;

    // log writeback
    ent = (nvpc_sync_wb_entry *)append_inode_n_log(inode, 1);
    if (!ent)
        goto out;
    
    ent->raw.type = NVPC_LOG_TYPE_WB;
    ent->raw.flags = 0;
    ent->raw.id = inode->nvpc_sync_ilog.log_cntr++;
    ent->committed_jid = jid;
    ent->file_offset_pg = index;
    arch_wb_cache_pmem((void*)ent, sizeof(nvpc_sync_write_entry));

    new_tail = inode->nvpc_sync_ilog.log_tail;

    pr_debug("[NVPC DEBUG]: wb work fn log\n");

    // clear PageNVPCPin
    BUG_ON(!pagecache_page);
    get_page(pagecache_page);
    // lock page first to prevent deadlock with nvpc_mark_page_writeback & nvpc_log_page_writeback
    lock_page(pagecache_page);
    /* 
     * Check if pagecache_page is still our pagecache, as it may already 
     * be evicted or migrated. If not so, we just leave the label there and 
     * do nothing. It is ok if we lost some ClearPageNVPCPDirty. 
     * Clear PageNVPCPDirty and PageNVPCPendingCopy both, once the writeback 
     * success, the persisted version is expired, and is not worth to do CoW.
     */
    if (pagecache_page->mapping && 
        pagecache_page->mapping->host == inode && 
        pagecache_page->index == index)
    {
        pr_debug("[NVPC DEBUG]: wb work fn clear pin\n");
        ClearPageNVPCPDirty(pagecache_page);
        ClearPageNVPCPendingCopy(pagecache_page);   // abandoned
    }
    unlock_page(pagecache_page);
    put_page(pagecache_page);

    pr_debug("[NVPC DEBUG]: wb work fn clear pin 1\n");

    write_commit(inode, old_tail, new_tail);
out:
    mutex_unlock(&inode->nvpc_sync_ilog.log_lock);
    pr_debug("[NVPC DEBUG]: wb work fn end\n");
}

struct nvpc_compact_one_control {
    int reclaim_d_pages;    // num of data pages reclaimed
    int reclaim_l_pages;    // num of log pages reclaimed
    int reclaim_goal;       // num of pages we want to reclaim, 0 for no limit
    bool distill_page;      // take available entry from existing log page and build new log page
    bool free_d_pages;      // free expired oop data pages and mark ip/oop entries as WRFRE
    // bool boot;              // we are during boot compact, feel free to do anything
    struct inode *_inode;
    nvpc_sync_log_entry *_working_tail_ent;
    int _jump;
};
typedef struct nvpc_compact_one_control nvpc_compact_one_control_t;


// We don't need to clwb those changed flags, because they don't need to be persisted. 
// Take special care of the ip entries with data crossing the log page boundary!!!
static bool __nvpc_compact_log_wkr(nvpc_sync_log_entry *ent, void *opaque)
{
    nvpc_compact_one_control_t *ncoc = (nvpc_compact_one_control_t *)opaque;
    void *xa_ent;
    pgoff_t index;
    unsigned long flags;

    // pr_info("[NVPC DEBUG] compacting ent %px type %d\n", ent, ent->type);

    // reaching the last working page, stop walking
    if (((uintptr_t)ent&PAGE_MASK) == ((uintptr_t)(ncoc->_working_tail_ent)&PAGE_MASK))
        return true;

    if (ncoc->_jump)
    {
        ncoc->_jump--;
        return false;
    }

    if (ent->type == NVPC_LOG_TYPE_ATTR)
        return false;
    
    if (ent->type == NVPC_LOG_TYPE_WB)
    {
        nvpc_sync_wb_entry *wbent = (nvpc_sync_wb_entry*)ent;
        nvpc_sync_page_info_t *info;
        index = wbent->file_offset_pg;
        xa_ent = xa_load(&ncoc->_inode->nvpc_sync_ilog.inode_log_pages, index);
        pr_debug("[NVPC DEBUG] compacting ent %px type WB xa %px\n", ent, xa_ent);
        if (!xa_ent)
            return false;
        info = (nvpc_sync_page_info_t*)xa_ent;
        if (info->committed_jid > info->abandoned_jid)
        {
            spin_lock_irqsave(&info->infolock, flags);
            if (info->committed_jid > info->abandoned_jid)
                info->abandoned_jid = info->committed_jid;
            spin_unlock_irqrestore(&info->infolock, flags);
        }
    }

    else if (ent->type == NVPC_LOG_TYPE_WRITE)
    {
        nvpc_sync_write_entry *went = (nvpc_sync_write_entry*)ent;
        nvpc_sync_page_info_t *info;

        if (!went->page_index)  // ip, set jump, recheck and set page_end
        {
            ncoc->_jump = (went->data_len + NVPC_LOG_ENTRY_SIZE - 1) / NVPC_LOG_ENTRY_SIZE;
        }

        index = went->file_offset >> PAGE_SHIFT;
        xa_ent = xa_load(&ncoc->_inode->nvpc_sync_ilog.inode_log_pages, index);
        pr_debug("[NVPC DEBUG] compacting ent %px type wr xa %px\n", ent, xa_ent);
        if (!xa_ent)
            return false;
        info = (nvpc_sync_page_info_t*)xa_ent;
        // unmarked entry, maybe expired and pending mark, maybe fresh
        // don't need the infolock, only this thread can access abandoned_jid
        if (went->raw.flags == 0)
        {
            // expired log ent
            if (went->jid < info->abandoned_jid)
            {
                went->raw.flags |= NVPC_LOG_FLAG_WREXP;
            }
            // fresh oop ent, update abandoned_jid
            else if (went->jid > info->abandoned_jid && went->page_index)
            {
                info->abandoned_jid = went->jid;
            }
        }
        

        if (went->raw.flags & NVPC_LOG_FLAG_WREXP)
        {
            // ip
            if (!went->page_index)
            {
                went->raw.flags |= NVPC_LOG_FLAG_WRCLR;
            }
            // oop, data page is still here
            else if (!(went->raw.flags & NVPC_LOG_FLAG_WRFRE))
            {
                if (ncoc->free_d_pages)
                {
                    nvpc_free_page(virt_to_page(nvpc_get_addr_pg(went->page_index)), 0);
                    went->raw.flags |= NVPC_LOG_FLAG_WRFRE;
                    went->raw.flags |= NVPC_LOG_FLAG_WRCLR;
                    ncoc->reclaim_d_pages++;
                }
            }
        }

        if (went->raw.flags & NVPC_LOG_FLAG_WRCLR)
        {
            int len = 1;
            if (!went->page_index)  // ip, add its data zone
                len += (went->data_len + NVPC_LOG_ENTRY_SIZE - 1) / NVPC_LOG_ENTRY_SIZE;
        }
        
    }

    // // TODO:
    // if (ncoc->distill_page)
    // {
        
    // }
    
    
    return false;
}

// return 0 for success
int nvpc_sync_compact_onehead(log_inode_head_entry *head, nvpc_compact_one_control_t *ncoc)
{
    struct block_device *bdev;
    struct super_block *sb;
    struct inode *inode;

    nvpc_sync_log_entry *first_log_ent = (nvpc_sync_log_entry *)nvpc_get_addr_pg(head->head_log_page);
    nvpc_sync_log_entry *working_tail_ent = head->committed_log_tail;

    /* find the inode */
    bdev = blkdev_get_by_dev(head->s_dev, FMODE_READ, NULL);
    WARN_ON(IS_ERR(bdev) || !bdev);
    sb = get_super(bdev);
    // WARN_ON(!sb);
    blkdev_put(bdev, FMODE_READ);
    if (!sb)
    {
        return -1;
    }
    inode = ilookup(sb, head->i_ino);
    // WARN_ON(!inode);
    if (!inode)
    {
        drop_super(sb);
        return -1;
    }

    pr_debug("[NVPC DEBUG] compact ino %lu from %px to %px ------------------------\n", inode->i_ino, first_log_ent, working_tail_ent);

    ncoc->_inode = inode;
    ncoc->_working_tail_ent = working_tail_ent;

    walk_log(first_log_ent, working_tail_ent, __nvpc_compact_log_wkr, ncoc, false);

// out:
    iput(inode);
    drop_super(sb);
    return 0;
}

struct npvc_compact_control {
    /* callee sets the following fields */
    int success_inodes;     // num of compacted inodes
    int fail_inodes;        // num of inodes that are broken
    int reclaim_d_pages;    // num of data pages reclaimed
    int reclaim_l_pages;    // num of log pages reclaimed
    
    /* caller sets the following fields */
    int reclaim_goal;       // num of pages we want to reclaim, 0 for no limit
    bool distill_page;      // take available entry from existing log page and build new log page
    bool free_d_pages;      // free expired oop data pages and mark ip/oop entries as WRFRE
};
typedef struct npvc_compact_control npvc_compact_control_t;

static bool __nvpc_inode_compact_wkr(nvpc_sync_head_entry *curr, void *opaque)
{
    nvpc_compact_one_control_t ncoc = {0};
    npvc_compact_control_t *ncc = (npvc_compact_control_t *)opaque;
    int ret;
    bool nolimit = (ncc->reclaim_goal == 0);

    ncoc.reclaim_goal = ncc->reclaim_goal;
    ncoc.distill_page = ncc->distill_page;
    ncoc.free_d_pages = ncc->free_d_pages;
    // ncoc.boot = false;
    ret = nvpc_sync_compact_onehead((log_inode_head_entry *)curr, &ncoc);
    if (unlikely(ret))  // should not happen
        ncc->fail_inodes++;
    else
        ncc->success_inodes++;
    
    ncc->reclaim_d_pages += ncoc.reclaim_d_pages;
    ncc->reclaim_l_pages += ncoc.reclaim_l_pages;

    // no limit, never stop
    if (nolimit)
        return false;
    
    ncc->reclaim_goal -= ncoc.reclaim_d_pages + ncoc.reclaim_l_pages;
    if (ncc->reclaim_goal <= 0)
        return true;
    
    return false;
}

// return 0 for success (no error)
int nvpc_sync_compact_all(npvc_compact_control_t *ncc)
{
    // maybe we can try to do reclaim evenly across all inodes

    walk_log_heads(__nvpc_inode_compact_wkr, ncc);
    if (ncc->reclaim_goal <= 0 && ncc->fail_inodes == 0)
        return 0;
    else
        // return some important info?
        return -1;
}

int nvpc_sync_compact_thread_fn(void *data)
{
    while (!kthread_should_stop()) {
        npvc_compact_control_t ncc = {0};
        pr_debug("nvpc compact start\n");
        ncc.free_d_pages = true;
        ncc.reclaim_goal = 0;
        nvpc_sync_compact_all(&ncc);
        pr_debug("nvpc compact done, succ_i %d, fail_i %d, r_logp %d, r_datap %d\n", 
            ncc.success_inodes, ncc.fail_inodes, ncc.reclaim_l_pages, ncc.reclaim_d_pages);
        msleep(nvpc_sync.compact_interval);
    }
    return 0;
}

bool nvpc_sync_detect()
{
    void *super_log_0 = nvpc_get_addr_pg(0);
    pr_debug("[NVPC DEBUG]: super log magic %#x\n", ((first_head_entry*)(super_log_0))->magic);
    return ((first_head_entry*)(super_log_0))->magic == NVPC_LOG_HEAD_MAGIC;
}

struct nvpc_rebuild_control
{
    int success_inodes;
    int fail_inodes;
    bool head_break;
};
typedef struct nvpc_rebuild_control nvpc_rebuild_control_t;

// struct __first_pass_state
// {
//     int _jump_entries;
//     nvpc_sync_log_entry *first_ne;
//     uint32_t jid_want;
//     nvpc_sync_log_entry *jid_first;
//     nvpc_sync_log_entry *jid_last;
//     struct xarray avail_logs;
//     nvpc_sync_attr_entry *latest_attr;
// };

// static bool __rebuild_first_pass_wkr(nvpc_sync_log_entry *ent, void *opaque)
// {
//     struct __first_pass_state *st = (struct __first_pass_state *)opaque;

//     if (st->_jump_entries)   // ip write, jump contents
//     {
//         st->_jump_entries--;
//         goto out;
//     }

//     // find the latest jid; find the latest attr

//     if (ent->type == NVPC_LOG_TYPE_WRITE && !(ent->flags & NVPC_LOG_FLAG_WREXP))
//     {
//         // record the first page/ent that has non-expired ent
//         if (st->first_ne == NULL)
//             st->first_ne = ent;
        
//         // store the entry in xarray index for third pass to find it
//         xa_store(&st->avail_logs, ((nvpc_sync_write_entry*)ent)->file_offset >> PAGE_SHIFT, ent, GFP_KERNEL);
//     }

//     if (ent->type == NVPC_LOG_TYPE_WRITE && ((nvpc_sync_write_entry*)ent)->page_index == 0) // ip write
//         st->_jump_entries = (((nvpc_sync_write_entry*)ent)->data_len + NVPC_LOG_ENTRY_SIZE - 1) / NVPC_LOG_ENTRY_SIZE;
    
//     // if (((nvpc_sync_write_entry*)ent)->jid != st->jid_want)
//     //     goto out;

//     switch (ent->type)
//     {
//     case NVPC_LOG_TYPE_WRITE:
//         if (((nvpc_sync_write_entry*)ent)->jid != st->jid_want)
//         {
//             st->jid_want = ((nvpc_sync_write_entry*)ent)->jid;
//             if (!st->jid_first)
//             {
//                 st->jid_first = ent;
//             }
//         }
//         st->jid_last = ent;
//         break;
//     case NVPC_LOG_TYPE_ATTR:
//         st->latest_attr = (nvpc_sync_attr_entry*)ent;
//         fallthrough;
//     default:
//         st->jid_want = 0;
//         st->jid_first = st->jid_last = NULL;
//         break;
//     }

// out:
//     return false;
// }

// struct __second_pass_state
// {
//     uint32_t jid_want;
//     int _jump_entries;
//     int err;
// };

// static bool __rebuild_second_pass_wkr(nvpc_sync_log_entry *ent, void *opaque)
// {
//     struct __second_pass_state *st = (struct __second_pass_state *)opaque;
//     nvpc_sync_write_entry* prev;

//     if (st->_jump_entries)  // ip write, jump contents
//     {
//         st->_jump_entries--;
//         return false;
//     }
    
//     // for each ent, check if it is write, but no matter if it is marked as WREXP
//     WARN_ON(ent->type != NVPC_LOG_TYPE_WRITE);
//     if (ent->type != NVPC_LOG_TYPE_WRITE)
//     {
//         st->err = 1;
//         return true;
//     }

//     WARN_ON(((nvpc_sync_write_entry*)ent)->jid != st->jid_want);
//     if (((nvpc_sync_write_entry*)ent)->jid != st->jid_want)
//     {
//         st->err = 2;
//         return true;
//     }

//     // check previous write, for oop, mark WREXP; for ip, chained mark WREXP
//     if (!((nvpc_sync_write_entry*)ent)->last_write) // no previous, move on
//         return false;
    
//     prev = (nvpc_sync_write_entry *)nvpc_get_addr(((nvpc_sync_write_entry*)ent)->last_write);
//     if (prev->page_index)  // oop
//     {
//         prev->raw.flags |= NVPC_LOG_FLAG_WREXP;
//         arch_wb_cache_pmem(ent, sizeof(nvpc_sync_write_entry*));
//     }
//     else    // ip
//     {
//         __nvpc_mark_free_chained_entries(prev, false, false);
//     }

//     if (!((nvpc_sync_write_entry*)ent)->page_index) // this ent is ip
//         st->_jump_entries = (((nvpc_sync_write_entry*)ent)->data_len + NVPC_LOG_ENTRY_SIZE - 1) / NVPC_LOG_ENTRY_SIZE;
    
//     return false;
// }

// struct __third_pass_state
// {
//     nvpc_sync_log_entry *jid_first;
//     nvpc_sync_log_entry *jid_last;
//     int _jump_entries;
// };


// static bool __rebuild_third_pass_wkr(nvpc_sync_log_entry *ent, void *opaque)
// {
//     struct __third_pass_state *st = (struct __third_pass_state *)opaque;

//     if (st->_jump_entries)   // ip write, jump contents
//     {
//         st->_jump_entries--;
//         goto out;
//     }

//     if (ent->type == NVPC_LOG_TYPE_WRITE && ((nvpc_sync_write_entry*)ent)->page_index == 0) // ip write
//         st->_jump_entries = (((nvpc_sync_write_entry*)ent)->data_len + NVPC_LOG_ENTRY_SIZE - 1) / NVPC_LOG_ENTRY_SIZE;

//     switch (ent->type)
//     {
//     case NVPC_LOG_TYPE_WRITE:
//         if (((nvpc_sync_write_entry*)ent)->jid != st->jid_want)
//         {
//             st->jid_want = ((nvpc_sync_write_entry*)ent)->jid;
//             if (!st->jid_first)
//             {
//                 st->jid_first = ent;
//             }
//         }
//         st->jid_last = ent;
//         break;
    
//     default:
//         st->jid_want = 0;
//         st->jid_first = st->jid_last = NULL;
//         break;
//     }

// out:
//     return false;
// }
struct file *make_recover_file(struct inode *inode)
{
    struct dentry *old_dent;
    struct file *old_fp;
    struct file *new_fp;
    char *newpath;

    old_dent = d_find_alias(inode);
    if (IS_ERR(old_dent))
        return ERR_CAST(old_dent);
    old_fp = filp_open(old_dent->d_name.name, O_RDONLY | O_LARGEFILE, 0);
    if (IS_ERR(old_fp))
    {
        pr_debug("[NVPC DEBUG]: rebuild make_recover_file open old file %s fail. \n", old_dent->d_name.name);
        dput(old_dent);
        return ERR_CAST(old_fp);
    }
    
    newpath = kmalloc(PATH_MAX, GFP_KERNEL);
    if (!newpath)
    {
        dput(old_dent);
        filp_close(old_fp, NULL);
        return ERR_PTR(-ENOMEM);
    }
    
    strcat(strcpy(newpath, old_dent->d_name.name), ".recover");
    dput(old_dent);
    
    new_fp = filp_open(newpath, O_CREAT | O_RDWR | O_LARGEFILE, 0);
    if (IS_ERR(new_fp))
    {
        pr_debug("[NVPC DEBUG]: rebuild make_recover_file open new file %s fail. \n", newpath);
        filp_close(old_fp, NULL);
        kfree(newpath);
        return ERR_CAST(new_fp);
    }

    vfs_copy_file_range(old_fp, 0, new_fp, 0, i_size_read(inode), 0);

    filp_close(old_fp, NULL);
    kfree(newpath);
    return new_fp;
}

// int _nvpc_inode_rebuild_withtrans(log_inode_head_entry *head)
// {
//     nvpc_sync_log_entry *ent;
//     struct __first_pass_state fps = {0};
//     struct __second_pass_state sps = {0};
//     // struct __third_pass_state tps = {0};

//     struct block_device *bdev;
//     struct super_block *sb;
//     struct inode *inode;

//     pgoff_t i;

//     struct file *rec_fp;
//     void *datapg, *maskpg;
//     datapg = (void*)get_zeroed_page(GFP_KERNEL);
//     if (!datapg)
//         return -1;
//     maskpg = (void*)get_zeroed_page(GFP_KERNEL);
//     if (!maskpg)
//     {
//         free_page((unsigned long)datapg);
//         return -1;
//     }

//     pr_info("[NVPC MSG]: rebuild worker work on blk dev %u:%u inode %d. \n", MAJOR(head->s_dev), MINOR(head->s_dev), (int)head->i_ino);

//     /* find the inode */
//     bdev = blkdev_get_by_dev(head->s_dev, FMODE_READ, NULL);
//     if (IS_ERR(bdev) || !bdev)
//     {
//         pr_info("[NVPC MSG]: rebuild worker cannot find block device %u:%u. \n", MAJOR(head->s_dev), MINOR(head->s_dev));
//         return -1;
//     }
//     sb = get_super(bdev);
//     // WARN_ON(!sb);
//     blkdev_put(bdev, FMODE_READ);
//     if (!sb)
//     {
//         pr_info("[NVPC MSG]: rebuild worker cannot find superblock on block device %u:%u. mount it first. \n", MAJOR(head->s_dev), MINOR(head->s_dev));
//         return -1;
//     }
//     inode = ilookup(sb, head->i_ino);
//     // WARN_ON(!inode);
//     if (!inode)
//     {
//         pr_info("[NVPC MSG]: rebuild worker cannot find inode %d on block device %u:%u. \n", (int)head->i_ino, MAJOR(head->s_dev), MINOR(head->s_dev));
//         drop_super(sb);
//         return -1;
//     }

//     ent = (nvpc_sync_log_entry *)head->head_log_page;
//     // NVTODO: walk log and do recover

//     // if the last entry is write, find that jid out
//     xa_init(&fps.avail_logs);
//     walk_log(ent, head->committed_log_tail, __rebuild_first_pass_wkr, &fps, false);
//     // iterate over the last jid and redo WREXP mark
//     if (fps.jid_want)
//     {
//         sps.jid_want = fps.jid_want;
//         walk_log(fps.jid_first, fps.jid_last, __rebuild_second_pass_wkr, &sps, false);
//         if (sps.err)
//         {
//             pr_debug("[NVPC DEBUG]: rebuild second pass error. \n");
//             goto err;
//         }
//     }

//     // NVTODO: copy current file to a new recover file
//     rec_fp = make_recover_file(inode);
//     if (IS_ERR(rec_fp))
//     {
//         // NVTODO: mark log head as half-done
//         pr_debug("[NVPC DEBUG]: rebuild fail to make recover file. \n");
//         goto err;
//     }
    
//     // for each page, if ent not marked as WREXP, redo entries (cross out(with same jid))
//     // NVNEXT: redo all entries that have same jid with the non-expired write
//     xa_for_each(&fps.avail_logs, i, ent) 
//     {
//         // struct page *page;
//         loff_t pos = ((nvpc_sync_write_entry*)ent)->file_offset & PAGE_MASK;
//         // if the entry is just marked in the first pass
//         if (ent->flags & NVPC_LOG_FLAG_WREXP)
//             continue;
        
//         nvpc_get_page_from_entry(
//             (nvpc_sync_write_entry*)ent, 
//             fps.latest_attr, datapg, maskpg);

//         // NVNEXT: apply maskpg to datapg
        
//         kernel_write(rec_fp, datapg, PAGE_SIZE, &pos);

//         // NVTODO: cut by i_size? not necessary
//         // NVTODO: write this page to file

//     }

//     filp_close(rec_fp, NULL);

//     free_page((unsigned long)datapg);
//     free_page((unsigned long)maskpg);
//     xa_destroy(&fps.avail_logs);
//     iput(inode);
//     drop_super(sb);
//     return 0;
// err:
//     free_page((unsigned long)datapg);
//     free_page((unsigned long)maskpg);
//     xa_destroy(&fps.avail_logs);
//     iput(inode);
//     drop_super(sb);
//     return -1;
// }

struct _rebuild_state
{
    struct xarray latest_pg_logs;
    nvpc_sync_attr_entry *latest_attr;
};

static bool __rebuild_page_chain_wkr(nvpc_sync_log_entry *ent, void *opaque)
{
    struct _rebuild_state *rs = (struct _rebuild_state*)opaque;
    // struct xarray *avail_logs = &rs->latest_pg_logs;
    
    if (ent->type == NVPC_LOG_TYPE_WRITE)
    {
        nvpc_sync_write_entry *went = ((nvpc_sync_write_entry*)ent);
        nvpc_sync_write_entry *prev = xa_load(&rs->latest_pg_logs, went->file_offset >> PAGE_SHIFT);
        went->last_write = prev?nvpc_get_off(prev):0;
        xa_store(&rs->latest_pg_logs, went->file_offset >> PAGE_SHIFT, went, GFP_KERNEL);
    }
    else if (ent->type == NVPC_LOG_TYPE_WB)
    {
        nvpc_sync_wb_entry *wbent = ((nvpc_sync_wb_entry*)ent);
        nvpc_sync_write_entry *prev = xa_load(&rs->latest_pg_logs, wbent->file_offset_pg);
        if (prev->jid > wbent->committed_jid)
        {
            // cut the chain
            while (prev && prev->last_write)
            {
                nvpc_sync_write_entry *tmp;
                tmp = (nvpc_sync_write_entry *)nvpc_get_addr(prev->last_write);
                if (tmp->jid < wbent->committed_jid)
                {
                    prev->last_write = 0;
                    tmp = 0;
                }
                prev = tmp;
            }
        }
    }
    else if (ent->type == NVPC_LOG_TYPE_ATTR)
    {
        nvpc_sync_attr_entry *aent = ((nvpc_sync_attr_entry*)ent);
        aent->last_attr = rs->latest_attr?nvpc_get_off(rs->latest_attr):0;
        rs->latest_attr = aent;
    }

    return false;
}

/*
 * return 1: oop
 * return 0: ip
 */
int __get_data_from_write_ent(nvpc_sync_write_entry *went, void *dst_page)
{
    // oop
    if (went->page_index)
    {
        // memcpy(dst_page, nvpc_get_addr_pg(went->page_index)+(went->file_offset&(~PAGE_MASK)), went->data_len);
        memcpy(dst_page, nvpc_get_addr_pg(went->page_index), PAGE_SIZE);
        return 1;
    }
    // ip
    else
    {
        struct iov_iter i;
        struct kvec kvec;
        kvec.iov_base = dst_page + ((went->file_offset)&(~PAGE_MASK));
        kvec.iov_len = went->data_len;
        iov_iter_kvec(&i, WRITE, &kvec, 1, went->data_len);
        walk_log((nvpc_sync_log_entry *)went+1, NULL, __copy_from_ip_log_wkr, &i, false);
        return 0;
    }
}

int _nvpc_inode_rebuild_fromstart(log_inode_head_entry *head)
{
    struct block_device *bdev;
    struct super_block *sb;
    struct inode *inode;

    // struct xarray avail_logs;
    struct _rebuild_state rs;

    struct file *rec_fp;

    pgoff_t i;
    nvpc_sync_write_entry *went;

    void *datapg, *maskpg, *tmppg;
    datapg = (void*)get_zeroed_page(GFP_KERNEL);
    if (!datapg)
        return -1;
    maskpg = (void*)get_zeroed_page(GFP_KERNEL);
    if (!maskpg)
    {
        free_page((unsigned long)datapg);
        return -1;
    }
    tmppg = (void*)get_zeroed_page(GFP_KERNEL);
    if (!tmppg)
    {
        free_page((unsigned long)datapg);
        free_page((unsigned long)maskpg);
        return -1;
    }

    /* find the inode */
    bdev = blkdev_get_by_dev(head->s_dev, FMODE_READ, NULL);
    if (IS_ERR(bdev) || !bdev)
    {
        pr_info("[NVPC MSG]: rebuild worker cannot find block device %u:%u. \n", MAJOR(head->s_dev), MINOR(head->s_dev));
        return -1;
    }
    sb = get_super(bdev);
    // WARN_ON(!sb);
    blkdev_put(bdev, FMODE_READ);
    if (!sb)
    {
        pr_info("[NVPC MSG]: rebuild worker cannot find superblock on block device %u:%u. mount it first. \n", MAJOR(head->s_dev), MINOR(head->s_dev));
        return -1;
    }
    inode = ilookup(sb, head->i_ino);
    // WARN_ON(!inode);
    if (!inode)
    {
        pr_info("[NVPC MSG]: rebuild worker cannot find inode %d on block device %u:%u. \n", (int)head->i_ino, MAJOR(head->s_dev), MINOR(head->s_dev));
        drop_super(sb);
        return -1;
    }

    // copy current file to a new recover file
    rec_fp = make_recover_file(inode);
    if (IS_ERR(rec_fp))
    {
        pr_debug("[NVPC DEBUG]: rebuild fail to make recover file. \n");
        goto err;
    }

    // 1. for each page, build a chain to link all its entries, meanwhile find the latest WRITEBACK
    xa_init(&rs.latest_pg_logs);
    rs.latest_attr = NULL;
    walk_log((nvpc_sync_log_entry *)nvpc_get_addr_pg(head->head_log_page), 
        head->committed_log_tail, __rebuild_page_chain_wkr, &rs, false);

    // 2. recover each page to the rebuild file
    xa_for_each(&rs.latest_pg_logs, i, went) 
    {
        loff_t pos;
        if (!went) continue;

        pos = went->file_offset & PAGE_MASK;
        kernel_read(rec_fp, datapg, PAGE_SIZE, &pos);
        memset(maskpg, 0, PAGE_SIZE);

        do
        {
            int off = went->file_offset & (PAGE_SIZE-1);
            int len = went->data_len;
            char *data_bytes = datapg;  // final page to rebuild
            char *mask_bytes = maskpg;  // mark already written bytes
            char *tmp_bytes = tmppg;    // extract data from current ent
            int enttype = __get_data_from_write_ent(went, tmppg);

            if (enttype)
            {
                // for oop ent, build the whole page
                len = PAGE_SIZE;
                off = 0;
            }
            
            while (len--)
            {
                if (!mask_bytes[off])
                {
                    data_bytes[off] = tmp_bytes[off];
                    mask_bytes[off] = 1;
                }
                off++;
            }

            if (enttype)
                break;  // stop at oop ent
            
            went = went->last_write ? 
                (nvpc_sync_write_entry *)nvpc_get_addr(went->last_write) : 
                0;
        } while (went);
        
        
        pos = went->file_offset & PAGE_MASK;
        kernel_write(rec_fp, datapg, PAGE_SIZE, &pos);

    }

    filp_close(rec_fp, NULL);

    free_page((unsigned long)datapg);
    free_page((unsigned long)maskpg);
    free_page((unsigned long)tmppg);
    xa_destroy(&rs.latest_pg_logs);
    iput(inode);
    drop_super(sb);
    return 0;
err:
    free_page((unsigned long)datapg);
    free_page((unsigned long)maskpg);
    free_page((unsigned long)tmppg);
    xa_destroy(&rs.latest_pg_logs);
    iput(inode);
    drop_super(sb);
    return -1;
}


#define nvpc_inode_rebuild _nvpc_inode_rebuild_fromstart

static bool __nvpc_inode_rebuild_wkr(nvpc_sync_head_entry *curr, void *opaque)
{
    nvpc_rebuild_control_t *nrc = (nvpc_rebuild_control_t *)opaque;
    log_inode_head_entry *head = (log_inode_head_entry *)curr;
    int ret;

    ret = nvpc_inode_rebuild(head);
    if (ret == -1)
        nrc->fail_inodes++;
    else
        nrc->success_inodes++;
    
    return false;
}

/*
 * first mount the super blocks with fsck, then run this function
 */
int nvpc_sync_rebuild()
{
    nvpc_rebuild_control_t nrc = {0};

    // walk heads
    walk_log_heads(__nvpc_inode_rebuild_wkr, &nrc);

    if (nrc.head_break)
        pr_info("[NVPC MSG]: log head is broken, fail to recover. \n");
    pr_info("[NVPC MSG]: rebuild done, %d file success, %d file fail. \n", nrc.success_inodes, nrc.fail_inodes);
    if (nrc.fail_inodes || nrc.head_break)
        return -1;
    return 0;
}


// NVTODO: add/drop page reference when setting/clearing PendingCopy to prevent eviction

// NVTODO: gracefully drop nvpc log when inode is deleted / closed successfully

// --- following are less important ---

// NVTODO: cancel page in mm/truncate.c

// NVTODO: metadata log for dir

// NVTODO: (demote to existing NVPC page to reduce memory usage) or remove existing page after demotion

// NVTODO: when OOM, fsync the inode and mark the inode as drained to prevent further access

// NVTODO: we can remove last_write, it's useless now // nooooope

// NVTODO: do more work in log compact