#include <linux/fs.h>
#include <linux/libnvdimm.h>
#include <linux/nvpc_rw.h>
#include <linux/nvpc_sync.h>
#include <linux/nvpc.h>

struct nvpc_sync nvpc_sync;

void init_sync_absorb_area(void)
{
    nvpc_sync.super_log_0 = nvpc_get_addr_pg(0);
    // pr_info("[NVPC DEBUG]: sl0: %#llx\n", nvpc_sync.super_log_0);

    memset(nvpc_sync.super_log_0, 0, PAGE_SIZE);
    ((first_head_entry*)(nvpc_sync.super_log_0))->magic = NVPC_LOG_HEAD_MAGIC;
    arch_wb_cache_pmem(nvpc_sync.super_log_0, PAGE_SIZE);
    nvpc_write_commit();

    mutex_init(&nvpc_sync.super_log_lock);
    // xa_init(&nvpc_sync.inode_log_heads);
}

typedef bool(*log_head_iterator)(nvpc_sync_head_entry *, void *);

/* walk through super log without lock */
static nvpc_sync_head_entry *walk_log_heads(log_head_iterator it, void *opaque)
{
    void *current_superlog = nvpc_sync.super_log_0;
    nvpc_sync_head_entry* log_head_i; // = (nvpc_sync_head_entry*)current_superlog;

    // pr_info("[NVPC DEBUG]: walk_log_heads entsz: %ld\n", sizeof(nvpc_sync_head_entry));
    // pr_info("[NVPC DEBUG]: walk_log_heads sl0: %#llx\n", nvpc_sync.super_log_0);

    /* first entry is for magic num */
    log_head_i = (nvpc_sync_head_entry*)current_superlog + 1;
    // pr_info("[NVPC DEBUG]: walk_log_heads ent1: %#llx\n", log_head_i);

walk_page:
    // pr_info("[NVPC DEBUG]: walk_log_heads ---page--- upper: %#llx\n", current_superlog + PAGE_SIZE - sizeof(nvpc_sync_head_entry));
    while ((void*)log_head_i < 
           current_superlog + PAGE_SIZE - sizeof(nvpc_sync_head_entry))
    {
        // pr_info("[NVPC DEBUG]: walk_log_heads lhi: %#llx\n", log_head_i);
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
        ne->flags = NVPC_LOG_HEAD_FLAG_NEXT;
        ne->next_sl_page = current_superlog = create_new_log_page();
        if (!current_superlog)
        {
            return NULL;
        }
        
        arch_wb_cache_pmem((void*)ne, sizeof(next_sl_page_entry));
        // nvpc_write_commit();

        return (nvpc_sync_head_entry *)current_superlog;
    }
}

/* iterator to find a log head for an inode */
static bool __find_log_inode_head_it(nvpc_sync_head_entry *ent, void *opaque)
{
    struct inode *ino = (struct inode *)opaque;
    log_inode_head_entry* lent = (log_inode_head_entry*)ent;
    return lent->s_dev == ino->i_sb->s_dev && lent->i_ino == ino->i_ino;
}

/* create the log and set up the log head */
static int init_log_inode_head(log_inode_head_entry * loghead, struct inode *inode)
{
    void *newlogpg;

    loghead->head_log_page = newlogpg = create_new_log_page();
    if (!newlogpg)
    {
        return -1;
    }
    loghead->s_dev = inode->i_sb->s_dev;
    loghead->i_ino = inode->i_ino;
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

    loghead = (log_inode_head_entry *)walk_log_heads(__find_log_inode_head_it, (void*)inode);
    // pr_info("[NVPC DEBUG]: walk_log_heads result: %#llx\n", loghead);
    if (loghead)
        return loghead;
    
    loghead = (log_inode_head_entry *)get_log_head_empty();
    // pr_info("[NVPC DEBUG]: get_log_head_empty result: %#llx\n", loghead);
    if (!loghead)
        return NULL;
    
    if (init_log_inode_head(loghead, inode)) {
        /* for get_log_head_empty */
        nvpc_write_commit();
        mutex_unlock(&nvpc_sync.super_log_lock);
        return NULL;
    }
    
    nvpc_write_commit();
    mutex_unlock(&nvpc_sync.super_log_lock);
    return loghead;
}

log_inode_head_entry *nvpc_get_log_inode(struct inode *inode)
{
    spin_lock(&inode->i_lock);
    if (!inode->nvpc_sync_ilog.log_head)
    {
        log_inode_head_entry *lh;
        spin_unlock(&inode->i_lock);
        lh = create_log_head(inode);
        if (!lh)
            return NULL;
        
        spin_lock(&inode->i_lock);
        inode->nvpc_sync_ilog.log_head = lh;
        inode->nvpc_sync_ilog.log_tail = lh->committed_log_tail;
    }
    spin_unlock(&inode->i_lock);
    return inode->nvpc_sync_ilog.log_head;
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
    int pg_ents_left = NVPC_PAGE_LOG_ENTRIES_LEFT(ent);

    /* we need more log pages, prepare them here */
    if (n_entries > pg_ents_left)
    {
        size_t need_pages = (n_entries - pg_ents_left + NVPC_PAGE_LOG_ENTRIES - 1) / NVPC_PAGE_LOG_ENTRIES;
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
                nextent->next_log_page = (uintptr_t)kaddr;
                nextent->raw.flags = NVPC_LOG_FLAG_NEXT;
                arch_wb_cache_pmem((void*)nextent, sizeof(nvpc_next_log_entry));
                nextent = NVPC_LOG_ENTRY_NEXT(kaddr);
            }
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
            nextent->next_log_page = (uintptr_t)newpage;
            nextent->raw.flags = NVPC_LOG_FLAG_NEXT;
            arch_wb_cache_pmem((void*)nextent, sizeof(nvpc_next_log_entry));
            last_page = newpage;
        }
        // nvpc_write_commit();
        tail_new_offset = (n_entries - pg_ents_left) % NVPC_PAGE_LOG_ENTRIES;
        /* preset log_tail to preserve n entries */
        inode->nvpc_sync_ilog.log_tail = (nvpc_sync_log_entry*)last_page + tail_new_offset;
    }
    else
    {
        /* preset log_tail to preserve n entries */
        inode->nvpc_sync_ilog.log_tail += n_entries;
    }

    /* return old tail as the first log to write to */
    if (ent == (nvpc_sync_log_entry*)NVPC_LOG_ENTRY_NEXT(ent))
    {
        ent = (nvpc_sync_log_entry*)((nvpc_next_log_entry*)ent)->next_log_page;
    }
    
    return ent;
}

/*
 * Move alongside the log for n entries
 */
// nvpc_sync_log_entry *advance_inode_n_log(nvpc_sync_log_entry *from, size_t n_entries)
// {

// }

typedef bool(*log_iterator)(nvpc_sync_log_entry *, void *);

static nvpc_sync_log_entry *walk_log(nvpc_sync_log_entry *from, 
                            nvpc_sync_log_entry *to, 
                            log_iterator it, void *opaque)
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
            if (it(current_log, opaque))
                return current_log;
            
            current_log++;
        }
        /* move to the next page */
        WARN_ON(!NVPC_LOG_HAS_NEXT(current_log));
        current_log = (nvpc_sync_log_entry *)NVPC_LOG_NEXT(current_log);
    }
    
    /* walk the last page */
    while (current_log < to)
    {
        if (it(current_log, opaque))
            return current_log;

        current_log++;
    }
    return current_log;
}

nvpc_sync_log_entry *write_oop(struct inode *inode, struct page *page, loff_t file_off)
{
    nvpc_sync_write_entry *ent;
    WARN_ON(!PageNVPC(page));
    // WARN_ON(inode->i_sb->s_nvpc_flags & SB_NVPC_ON);

    ent = (nvpc_sync_write_entry *)append_inode_n_log(inode, 1);
    if (!ent)
        return NULL;
    
    ent->file_offset = file_off;
    ent->data_len = PAGE_SIZE;
    ent->page_index = nvpc_get_off_pg(page_to_virt(page));
    arch_wb_cache_pmem((void*)ent, sizeof(nvpc_sync_write_entry));
    
    inode->nvpc_sync_ilog.log_head->committed_log_tail = inode->nvpc_sync_ilog.log_tail;
    arch_wb_cache_pmem(
        (void*)&inode->nvpc_sync_ilog.log_head->committed_log_tail, 
        sizeof(nvpc_sync_write_entry*)
    );
    
    nvpc_write_commit();

    // update xarray
    xa_store(&inode->nvpc_sync_ilog.inode_log_pages, file_off, ent, GFP_KERNEL);

    return inode->nvpc_sync_ilog.log_head->committed_log_tail;
}

static bool __write_ioviter_to_log_it(nvpc_sync_log_entry *ent, void *opaque)
{
    struct iov_iter *from = (struct iov_iter *)opaque;
    _copy_from_iter_flushcache(ent, NVPC_LOG_ENTRY_SIZE, from);
    // WARN_ON(wrsz > NVPC_LOG_ENTRY_SIZE);
    return false;
}

// NVTODO
nvpc_sync_log_entry *write_ip(struct inode *inode, struct iov_iter *from, loff_t file_off)
{
    size_t len = iov_iter_count(from);
    size_t nentries = (len + NVPC_LOG_ENTRY_SIZE - 1) / NVPC_LOG_ENTRY_SIZE + 1;
    nvpc_sync_write_entry *ent0;
    nvpc_sync_log_entry *last;
    // WARN_ON(inode->i_sb->s_nvpc_flags & SB_NVPC_ON);
    ent0 = (nvpc_sync_write_entry *)append_inode_n_log(inode, nentries);
    if (!ent0)
        return NULL;
    
    last = walk_log((nvpc_sync_log_entry *)(ent0+1), inode->nvpc_sync_ilog.log_tail, 
                __write_ioviter_to_log_it, from);
    WARN_ON(last != inode->nvpc_sync_ilog.log_tail);
    
    ent0->file_offset = file_off;
    ent0->data_len = len;
    ent0->page_index = 0;
    arch_wb_cache_pmem((void*)ent0, sizeof(nvpc_sync_write_entry));

    inode->nvpc_sync_ilog.log_head->committed_log_tail = inode->nvpc_sync_ilog.log_tail;
    arch_wb_cache_pmem(
        (void*)&inode->nvpc_sync_ilog.log_head->committed_log_tail, 
        sizeof(nvpc_sync_write_entry*)
    );

    nvpc_write_commit();

    return inode->nvpc_sync_ilog.log_head->committed_log_tail;
}

// NVTODO: log compact

// NVTODO: log replay