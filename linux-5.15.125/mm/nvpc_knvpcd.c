/*
 * NVPC: A page cache extention with non-volatile memory.
 * This is a prototype and may be unsafe now.
 * 
 * knvpcd: a kernel thread to shrink pages from NVPC like
 * kswapd.
 */

#include <linux/nvpc.h>

#include <linux/printk.h>
#include <linux/dax.h>

#include <linux/swap.h>
#include <linux/module.h>
#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/vmstat.h>
#include <linux/pagevec.h>
#include <linux/pagemap.h>
#include <linux/swapops.h>

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

// for knvpcd
#include <linux/freezer.h>
#include <linux/cpu.h>
#include <linux/cpuset.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/rmap.h>
#include <linux/xarray.h>

#include "internal.h"

struct scan_control {
	/* How many pages shrink_list() should reclaim */
	unsigned long nr_to_reclaim;

	/*
	 * Nodemask of nodes allowed by the caller. If NULL, all nodes
	 * are scanned.
	 */
	nodemask_t	*nodemask;

	/* Writepage batching in laptop mode; RECLAIM_WRITE */
	unsigned int may_writepage:1;

	/* Can mapped pages be reclaimed? */
	unsigned int may_unmap:1;

	/* Can pages be swapped as part of reclaim? */
	unsigned int may_swap:1;

	/* determine whether it should promote NVPC pages only */
	unsigned int nvpc_promote:1;

	/* determine whether it should evict NVPC pages */
	unsigned int nvpc_evict:1;

	/* Allocation order */
	s8 order;

	/* Incremented by the number of inactive pages that were scanned */
	unsigned long nr_scanned;

	/* Number of pages freed so far during a call to shrink_zones() */
	unsigned long nr_reclaimed;

	struct {
		unsigned int dirty;
		unsigned int unqueued_dirty;
		unsigned int congested;
		unsigned int writeback;
		unsigned int immediate;
		unsigned int file_taken;
		unsigned int taken;
	} nr;

};

#ifdef ARCH_HAS_PREFETCHW
#define prefetchw_prev_lru_page(_page, _base, _field)			\
	do {								\
		if ((_page)->lru.prev != _base) {			\
			struct page *prev;				\
									\
			prev = lru_to_page(&(_page->lru));		\
			prefetchw(&prev->_field);			\
		}							\
	} while (0)
#else
#define prefetchw_prev_lru_page(_page, _base, _field) do { } while (0)
#endif

enum page_references {
	PAGEREF_RECLAIM,
	PAGEREF_RECLAIM_CLEAN,
	PAGEREF_KEEP,
	PAGEREF_ACTIVATE, // NVTODO: promote???
};

/**
 * @brief Originate from vmscan.c: page_check_references(), no change
 * 
 * @param page 
 * @param sc 
 * @return enum page_references 
 */
static enum page_references page_check_references(struct page *page,
						  struct scan_control *sc)
{
	int referenced_ptes, referenced_page;
	unsigned long vm_flags;

	referenced_ptes = page_referenced(page, 1, NULL, &vm_flags); // NVTODO: check this NULL (memcg)
	referenced_page = TestClearPageReferenced(page); // previous reference

	/*
	 * Mlock lost the isolation race with us.  Let try_to_unmap()
	 * move the page to the unevictable list.
	 */
	if (vm_flags & VM_LOCKED)
		return PAGEREF_RECLAIM;

	if (referenced_ptes) {
		/*
		 * All mapped pages start out with page table
		 * references from the instantiating fault, so we need
		 * to look twice if a mapped file page is used more
		 * than once.
		 *
		 * Mark it and spare it for another trip around the
		 * inactive list.  Another page table reference will
		 * lead to its activation.
		 *
		 * Note: the mark is set for activated pages as well
		 * so that recently deactivated but used pages are
		 * quickly recovered.
		 */
		SetPageReferenced(page);

		if (referenced_page || referenced_ptes > 1)
			return PAGEREF_ACTIVATE;

		/*
		 * Activate file-backed executable pages after first usage.
		 */
		if ((vm_flags & VM_EXEC) && !PageSwapBacked(page))
			return PAGEREF_ACTIVATE;

		return PAGEREF_KEEP;
	}

	/* Reclaim if clean, defer dirty pages to writeback */
	if (referenced_page && !PageSwapBacked(page))
		return PAGEREF_RECLAIM_CLEAN;

	return PAGEREF_RECLAIM;
}

/**
 * @brief Originate from vmscan.c: demote_page_list()
 * 
 * @param nvpc_pages 
 * @return unsigned int 
 */
static unsigned int promote_pages_from_nvpc(struct list_head *nvpc_pages)
{
	unsigned int nr_succeeded;
	struct nvpc *nvpc;
	int err;

	nvpc = get_nvpc();

	if (list_empty(nvpc_pages))
		return 0;

	// read_lock_irq(&nvpc->meta_lock);
	if (!nvpc->enabled && !nvpc->extend_lru)
	{
		nr_succeeded = 0;
		// read_unlock_irq(&nvpc->meta_lock);
		goto out;
	}
	
	// want to use migration like in compaction process, rather than NUMA process
	err = migrate_pages(nvpc_pages, nvpc_alloc_promote_page, NULL, 0, 
						MIGRATE_ASYNC, MR_NVPC_LRU_PROMOTE, &nr_succeeded);
	// read_unlock_irq(&nvpc->meta_lock);

	// if (current_is_kswapd())
	// 	__count_vm_events(PGNVPC_PROMOTE_KSWAPD, nr_succeeded);
	// else
	// 	__count_vm_events(PGNVPC_PROMOTE_DIRECT, nr_succeeded);

out:
	return nr_succeeded;
}

static bool nvpc_can_promote(int nid, struct scan_control *sc) 
{
	/* NVPC is independent to NUMA subsystem, switch can not be used here */
	// if (!numa_demotion_enabled)
	// 	return false;
	if (sc) {
		if (sc->nvpc_promote)
			return true;
		/* It is pointless to do demotion in memcg reclaim */
		/* NVPC has no relation to memcg reclaim */
		// if (cgroup_reclaim(sc))
		// 	return false;
	}
	return false;
}

/*
 * nvpc_move_pages_to_lru_promote() moves pages from private @list to appropriate LRU list.
 * On return, @list is reused as a list of pages to be freed by the caller.
 *
 * Returns the number of pages moved to the given lruvec.
 */
static unsigned int nvpc_move_pages_to_lru_promote(struct lruvec *lruvec,
				      struct list_head *list)
{
	int nr_pages, nr_moved;
	struct page *page;

	nr_pages = 0;
	nr_moved = 0;

	while (!list_empty(list)) {
		page = lru_to_page(list);
		VM_BUG_ON_PAGE(PageLRU(page), page);
		list_del(&page->lru);

		// If page is inevictable, putback it to LRU
		if (unlikely(!page_evictable(page))) {
			spin_unlock_irq(&lruvec->lru_lock);
			putback_lru_page(page);
			spin_lock_irq(&lruvec->lru_lock);
			continue;
		}

		SetPageLRU(page); // NVTODO: where to unset this flag?

		VM_BUG_ON_PAGE(!page_matches_lruvec(page, lruvec), page);

		// NVBUG: The page here must be NVPC page, cuz we only keep NVPC pages in the list
		VM_BUG_ON_PAGE(!PageNVPC(page), page);
		add_page_to_lru_list(page, lruvec);

		nr_pages = thp_nr_pages(page);
		nr_moved += nr_pages;
		if (PageActive(page))
			workingset_age_nonresident(lruvec, nr_pages);
	}
	return nr_moved;
}


/**
 * @brief validate the page is NVPC-compatible
 * 
 * @param page
 */
static void nvpc_check_page(struct page * page)
{
	VM_BUG_ON_PAGE(!PageNVPC(page), page); // must be NVPC

	VM_BUG_ON_PAGE(PageActive(page), page);
	VM_BUG_ON_PAGE(PageTransHuge(page), page);
	VM_BUG_ON_PAGE(PageSwapBacked(page), page);
	VM_BUG_ON_PAGE(PageCompound(page), page);
	VM_BUG_ON_PAGE(PageAnon(page), page);
	VM_BUG_ON_PAGE(page_mapped(page), page);
	VM_BUG_ON_PAGE(!page_is_file_lru(page), page); // page should be in file LRU

	// VM_BUG_ON_PAGE(PageUnevictable(page), page);
	// VM_BUG_ON_PAGE(PageLRU(page), page);

	// VM_BUG_ON_PAGE(PageSwapCache(page), page);
	// VM_BUG_ON_PAGE(PageMappedToDisk(page), page);
	// VM_BUG_ON_PAGE(PageMlocked(page), page);
	// VM_BUG_ON_PAGE(PageKsm(page), page);
	// VM_BUG_ON_PAGE(PageHuge(page), page);
	// VM_BUG_ON_PAGE(PageTail(page), page);
	// VM_BUG_ON_PAGE(PageDoubleMap(page), page);
	// VM_BUG_ON_PAGE(PagePrivate(page), page);
	// VM_BUG_ON_PAGE(PageLocked(page), page);
}

/**
 * @brief Originate from vmscan.c: shrink_page_list(), large scale modification
 * @note for NVPC promition only
 * 
 * @param page_list 
 * @param pgdat 
 * @param sc 
 * @param ignore_references 
 * @return unsigned int 
 */
unsigned int promote_page_list(struct list_head *page_list,
				     struct pglist_data *pgdat,
				     struct scan_control *sc,
				     bool ignore_references)
{
	LIST_HEAD(ret_pages);
	LIST_HEAD(nvpc_promote_pages);

	unsigned int nr_reclaimed = 0;

	bool do_promote_pass;
	bool do_nvpc_pass;
	struct nvpc *nvpc = get_nvpc();
	/* judge only by sc->nvpc_promote */
	/* If promotion is set, other operations will not be done */
	do_promote_pass = nvpc_can_promote(pgdat->node_id, sc); // exclusive promotion
	do_nvpc_pass = nvpc->enabled && nvpc->extend_lru;

	cond_resched();

retry:
	while (!list_empty(page_list)) {
		struct page *page;

		cond_resched();

		page = lru_to_page(page_list);
		list_del(&page->lru);

		if (!trylock_page(page)) // whether this page is locked by others
		{
			pr_info("[KNVPCD WARN] trylock page failed! Keep it from promotion.\n"); // NVTODO: check whether this is a bug
			goto keep;
		}

		VM_BUG_ON_PAGE(PageActive(page), page); // must be inactive

		sc->nr_scanned += compound_nr(page);

		/* NVPC check */
		nvpc_check_page(page);

		/* NVPC promotion */
		if (do_nvpc_pass && do_promote_pass) {
			u8 nvpc_lru_cnt = page_nvpc_lru_cnt(page);
			if (get_nvpc()->promote_level && nvpc_lru_cnt >= get_nvpc()->promote_level) {
				list_add(&page->lru, &nvpc_promote_pages);
				unlock_page(page);
				continue;
				// NVTODO: deal with workingset?
			}
		}

		unlock_page(page);
keep:
		list_add(&page->lru, &ret_pages);
		VM_BUG_ON_PAGE(PageLRU(page) || PageUnevictable(page), page);
	}
	/* 'page_list' is always empty here */

	/* Promote: Migrate NVPC pages to DRAM, but not in LRU */
	if (do_promote_pass) {
		nr_reclaimed += promote_pages_from_nvpc(&nvpc_promote_pages);
		if (!list_empty(&nvpc_promote_pages))
		{
			list_splice_init(&nvpc_promote_pages, page_list); // page left unprocessed should be NVPC pages and reprocessed
			// do_nvpc_pass = false;
			goto retry; // No other methods can be used
		}
	}

	list_splice(&ret_pages, page_list); // page left unprocessed, keeped NVPC pages

	return nr_reclaimed;
}


static int too_many_isolated_nvpc(struct pglist_data *pgdat)
{
	unsigned long nvpc_lru, isolated;

	if (current_is_kswapd())
		return 0;

	nvpc_lru = node_page_state(pgdat, NR_NVPC_FILE);
	isolated = node_page_state(pgdat, NR_ISOLATED_NVPC);

	return nvpc_lru > isolated;
}

// NVTODO: nr_to_scan is not used
static unsigned long
promote_nvpc_list(struct lruvec *lruvec, struct scan_control *sc)
{
	LIST_HEAD(page_list);
	unsigned long nr_promoted;
	unsigned long nr_taken;
	struct pglist_data *pgdat = lruvec_pgdat(lruvec);
	bool stalled = false;

	while (unlikely(too_many_isolated_nvpc(pgdat))) {
		if (stalled)
			return 0;

		/* wait a bit for the reclaimer. */
		msleep(100);
		stalled = true;

		/* We are about to die and free our memory. Return now. */
		if (fatal_signal_pending(current))
			return SWAP_CLUSTER_MAX;
	}

	/* in fact, nvpc pages will never be inside a pvec, so we don't drain */
	// lru_add_drain();

	spin_lock_irq(&lruvec->lru_lock);
	nr_taken = nvpc_promote_vec_isolate(&page_list, lruvec);
	__mod_node_page_state(pgdat, NR_ISOLATED_NVPC, nr_taken);
	spin_unlock_irq(&lruvec->lru_lock);

	if (nr_taken == 0)
		return 0;

	nr_promoted = promote_page_list(&page_list, pgdat, sc, false);

	spin_lock_irq(&lruvec->lru_lock);
	nr_taken = nvpc_move_pages_to_lru_promote(lruvec, &page_list); // After this, page_list will be empty
	__mod_node_page_state(pgdat, NR_ISOLATED_NVPC, -nr_taken);
	spin_unlock_irq(&lruvec->lru_lock);

	// NVBUG: free nvpc pages that we won't use here?

	pr_info("[knvpcd nr info promote] nr_promoted: %lu, nr_taken: %lu\n", 
			nr_promoted, nr_taken);
	return nr_promoted;
}








/*
 * nvpc_move_pages_to_lru_reclaim() moves pages from private @list to appropriate LRU list.
 * On return, @list is reused as a list of pages to be freed by the caller.
 *
 * Returns the number of pages moved to the given lruvec.
 */
static unsigned int nvpc_move_pages_to_lru_reclaim(struct lruvec *lruvec,
				      struct list_head *list)
{
	int nr_pages, nr_moved;
	LIST_HEAD(pages_to_free);
	struct page *page;

	nr_pages = 0;
	nr_moved = 0;

	while (!list_empty(list)) {
		page = lru_to_page(list);
		VM_BUG_ON_PAGE(PageLRU(page), page);
		list_del(&page->lru);

		// If page is inevictable, putback it to LRU
		if (unlikely(!page_evictable(page))) {
			spin_unlock_irq(&lruvec->lru_lock);
			putback_lru_page(page);
			spin_lock_irq(&lruvec->lru_lock);
			continue;
		}

		SetPageLRU(page); // NVTODO: where to unset this flag?

		if (unlikely(put_page_testzero(page))) { // NVTODO: whether page refcnt == 0 will be freed by NVPC-related function
			__clear_page_lru_flags(page);
			list_add(&page->lru, &pages_to_free);
			continue;
		}

		VM_BUG_ON_PAGE(!page_matches_lruvec(page, lruvec), page);

		// NVBUG: The page here must be NVPC page, cuz we only keep NVPC pages in the list
		VM_BUG_ON_PAGE(!PageNVPC(page), page);
		add_page_to_lru_list(page, lruvec);

		nr_pages = thp_nr_pages(page);
		nr_moved += nr_pages;
		if (PageActive(page))
			workingset_age_nonresident(lruvec, nr_pages);
	}

	list_splice(&pages_to_free, list);

	return nr_moved;
}


/**
 * @brief Originate from vmscan.c: isolate_lru_pages(), for nvpc eviction use only
 * 
 * @param nr_to_scan 
 * @param lruvec 
 * @param dst 
 * @param nr_scanned 
 * @param sc 
 * @return unsigned long 
 * 
 * NVTODO: united page isolation
 */
static unsigned long nvpc_isolate_evict_pages(unsigned long nr_to_scan,
		struct lruvec *lruvec, struct list_head *dst,
		unsigned long *nr_scanned, struct scan_control *sc)
{
	struct list_head *src = &lruvec->lists[LRU_NVPC_FILE]; // NVPC LRU
	unsigned long nr_taken = 0;
	unsigned long scan, total_scan, nr_pages;
	LIST_HEAD(pages_skipped);

	total_scan = 0;
	scan = 0;
	while (scan < nr_to_scan && !list_empty(src)) {
		struct list_head *move_to = src;
		struct page *page;

		page = lru_to_page(src);
		prefetchw_prev_lru_page(page, src, flags); // NVXXX: enable prefetchw on NVDIMMS?

		nr_pages = compound_nr(page);
		total_scan += nr_pages;

		if (!PageNVPC(page)) {
			move_to = &pages_skipped;
			goto move;
		}

		/*
		 * Do not count skipped pages because that makes the function
		 * return with no isolated pages if the LRU mostly contains
		 * ineligible pages.  This causes the VM to not reclaim any
		 * pages, triggering a premature OOM.
		 * Account all tail pages of THP.
		 */
		scan += nr_pages;

		if (!PageLRU(page)) // NVTODO: LRU include NVPC LRU?
			goto move;
		if (!sc->may_unmap && page_mapped(page))
			goto move;

		/*
		 * Be careful not to clear PageLRU until after we're
		 * sure the page is not being freed elsewhere -- the
		 * page release code relies on it.
		 */
		if (unlikely(!get_page_unless_zero(page)))
			goto move;

		if (!TestClearPageLRU(page)) {
			put_page(page);
			goto move;
		}

		nr_taken += nr_pages;
		move_to = dst;
move:
		list_move(&page->lru, move_to);
		update_lru_size(lruvec, page_lru(page), page_zonenum(page),
			-thp_nr_pages(page));
	}

	/*
	 * Splice any skipped pages to the start of the LRU list. 
	 * Note that this disrupts the LRU order when reclaiming for lower zones but
	 * we cannot splice to the tail. If we did then the SWAP_CLUSTER_MAX
	 * scanning would soon rescan the same pages to skip and put the
	 * system at risk of premature OOM.
	 */
	if (!list_empty(&pages_skipped)) {
		list_splice(&pages_skipped, src);
	}
	*nr_scanned = total_scan;
	// update_lru_sizes(lruvec, lru, nr_zone_taken);
	return nr_taken;
}


/*
 * Same as remove_mapping, but if the page is removed from the mapping, it
 * gets returned with a refcount of 0.
 */

/**
 * @brief Originate from vmscan.c: __remove_mapping()
 * 
 * @param mapping 
 * @param page 
 * @param reclaimed 
 * @param target_memcg 
 * @return int 
 */
static int __remove_mapping(struct address_space *mapping, struct page *page,
			    bool reclaimed, struct mem_cgroup *target_memcg)
{
	int refcount;
	void *shadow = NULL;

	BUG_ON(!PageLocked(page));
	BUG_ON(mapping != page_mapping(page));

	xa_lock_irq(&mapping->i_pages);
	/*
	 * The non racy check for a busy page.
	 *
	 * Must be careful with the order of the tests. When someone has
	 * a ref to the page, it may be possible that they dirty it then
	 * drop the reference. So if PageDirty is tested before page_count
	 * here, then the following race may occur:
	 *
	 * get_user_pages(&page);
	 * [user mapping goes away]
	 * write_to(page);
	 *				!PageDirty(page)    [good]
	 * SetPageDirty(page);
	 * put_page(page);
	 *				!page_count(page)   [good, discard it]
	 *
	 * [oops, our write_to data is lost]
	 *
	 * Reversing the order of the tests ensures such a situation cannot
	 * escape unnoticed. The smp_rmb is needed to ensure the page->flags
	 * load is not satisfied before that of page->_refcount.
	 *
	 * Note that if SetPageDirty is always performed via set_page_dirty,
	 * and thus under the i_pages lock, then this ordering is not required.
	 */
	refcount = 1 + compound_nr(page);
	if (!page_ref_freeze(page, refcount))
		goto cannot_free;
	/* note: atomic_cmpxchg in page_ref_freeze provides the smp_rmb */
	if (unlikely(PageDirty(page))) {
		page_ref_unfreeze(page, refcount);
		goto cannot_free;
	}

	if (PageSwapCache(page)) {
		swp_entry_t swap = { .val = page_private(page) };
		mem_cgroup_swapout(page, swap);
		if (reclaimed && !mapping_exiting(mapping))
			shadow = workingset_eviction(page, target_memcg);
		__delete_from_swap_cache(page, swap, shadow);
		xa_unlock_irq(&mapping->i_pages);
		put_swap_page(page, swap);
	} else {
		void (*freepage)(struct page *);

		freepage = mapping->a_ops->freepage;
		/*
		 * Remember a shadow entry for reclaimed file cache in
		 * order to detect refaults, thus thrashing, later on.
		 *
		 * But don't store shadows in an address space that is
		 * already exiting.  This is not just an optimization,
		 * inode reclaim needs to empty out the radix tree or
		 * the nodes are lost.  Don't plant shadows behind its
		 * back.
		 *
		 * We also don't store shadows for DAX mappings because the
		 * only page cache pages found in these are zero pages
		 * covering holes, and because we don't want to mix DAX
		 * exceptional entries and shadow exceptional entries in the
		 * same address_space.
		 */
		if (reclaimed && page_is_file_lru(page) &&
		    !mapping_exiting(mapping) && !dax_mapping(mapping))
			shadow = workingset_eviction(page, target_memcg);
		__delete_from_page_cache(page, shadow);
		xa_unlock_irq(&mapping->i_pages);

		if (freepage != NULL)
			freepage(page);
	}

	return 1;

cannot_free:
	xa_unlock_irq(&mapping->i_pages);
	return 0;
}


/**
 * @brief Originate from vmscan.c: shrink_page_list(), large scale modification
 * 
 * @param page_list 
 * @param pgdat 
 * @param sc 
 * @param ignore_references 
 * @return unsigned int 
 */
static unsigned int shrink_page_list(struct list_head *page_list,
				     struct pglist_data *pgdat,
				     struct scan_control *sc,
				     bool ignore_references)
{
	LIST_HEAD(ret_pages); // return page_list, no change
	LIST_HEAD(free_nvpc_pages); // NVPC pages should be freed

	unsigned int nr_reclaimed;
	nr_reclaimed = 0;

	cond_resched();

	while (!list_empty(page_list)) {
		struct address_space *mapping; 
		struct page *page;
		enum page_references references = PAGEREF_RECLAIM;
		unsigned int nr_pages;

		cond_resched();

		page = lru_to_page(page_list);
		list_del(&page->lru);

		if (!trylock_page(page))
		{
			pr_info("[KNVPCD WARN] trylock page failed! Keep it from eviction.\n");
			goto keep;
		}

		/* After this barrier, page is locked by trylock_page(page) */

		nvpc_check_page(page);
		nr_pages = compound_nr(page);
		VM_BUG_ON(nr_pages > 1); // THP cannot be NVPC. must be 1
		sc->nr_scanned += nr_pages;

		if (unlikely(!page_evictable(page))) // Usually, file-backed page is evictable
			goto activate_locked;

		if (PageWriteback(page))
			goto activate_locked;

		mapping = page_mapping(page);

		if (!ignore_references)
			references = page_check_references(page, sc);

		switch (references) {
			case PAGEREF_ACTIVATE: // NVTODO: 2nd chance for NVPC
				goto activate_locked;
			case PAGEREF_KEEP:
				goto keep_locked;
			case PAGEREF_RECLAIM:
			case PAGEREF_RECLAIM_CLEAN:
				; /* try to reclaim the page below */
		}

		if (PageDirty(page)) {
			if (page_is_file_lru(page) && 
				(!PageReclaim(page) || !test_bit(PGDAT_DIRTY, &pgdat->flags))) 
				goto activate_locked;
			goto keep_locked;
		}

		/*
		 * If the page has buffers, try to free the buffer mappings
		 * associated with this page. If we succeed we try to free
		 * the page as well.
		 *
		 * We do this even if the page is PageDirty().
		 * try_to_release_page() does not perform I/O, but it is
		 * possible for a page to have PageDirty set, but it is actually
		 * clean (all its buffers are clean).  This happens if the
		 * buffers were written out directly, with submit_bh(). ext3
		 * will do this, as well as the blockdev mapping.
		 * try_to_release_page() will discover that cleanness and will
		 * drop the buffers and mark the page clean - it can be freed.
		 *
		 * Rarely, pages can have buffers and no ->mapping.  These are
		 * the pages which were not successfully invalidated in
		 * truncate_cleanup_page().  We try to drop those buffers here
		 * and if that worked, and the page is no longer mapped into
		 * process address space (page_count == 1) it can be freed.
		 * Otherwise, leave the page on the LRU so it is swappable.
		 */
		//  NVTODO: test this block, cannot free it with private mapping
		if (page_has_private(page)) {
			if (!try_to_release_page(page, GFP_KERNEL))
				goto activate_locked;
			if (!mapping && page_count(page) == 1) {
				unlock_page(page);
				if (put_page_testzero(page))
					goto free_it;
				else {
					/*
					 * rare race with speculative reference.
					 * the speculative reference will free
					 * this page shortly, so we may
					 * increment nr_reclaimed here (and
					 * leave it off the LRU).
					 */
					nr_reclaimed++;
					continue;
				}
			}
		}

		if (!mapping || !__remove_mapping(mapping, page, true, NULL)) // NVTODO: check this NULL (memcg)
			goto keep_locked;

		unlock_page(page);
free_it:
		pr_info("[NVPC knvpcd evict] free NVPC page: %p\n", page);
		nr_reclaimed += nr_pages;
		list_add(&page->lru, &free_nvpc_pages);
		continue;

activate_locked:
		; // NVTODO: 2nd chance for NVPC

keep_locked:
		unlock_page(page);
keep:
		list_add(&page->lru, &ret_pages);
		VM_BUG_ON_PAGE(PageLRU(page) || PageUnevictable(page), page);
	}

	/* 'page_list' is always empty here */

	try_to_unmap_flush();
	// NVTODO: free_unref_nvpc_page_list(&free_nvpc_pages);
	nvpc_free_pages(&free_nvpc_pages);

	list_splice(&ret_pages, page_list);

	return nr_reclaimed;
}

/**
 * @brief Originate from vmscan.c: shrink_inactive_list(), include evict and writeback
 * 
 * @param lruvec 
 * @param sc 
 * @return unsigned long 
 * 
 * NVTODO: second chance should be added
 */
static unsigned long
shrink_nvpc_list(struct lruvec *lruvec, struct scan_control *sc)
{
	LIST_HEAD(page_list);
	unsigned long nr_reclaimed;
	unsigned long nr_taken;
	unsigned long nr_to_scan;
	unsigned long nr_scanned;
	struct pglist_data *pgdat;
	bool stalled;

	stalled = false;
	pgdat = lruvec_pgdat(lruvec);
	nr_to_scan = sc->nr_to_reclaim;

	while (unlikely(too_many_isolated_nvpc(pgdat))) {
		if (stalled)
			return 0;

		/* wait a bit for the reclaimer. */
		msleep(100);
		stalled = true;

		/* We are about to die and free our memory. Return now. */
		if (fatal_signal_pending(current))
			return SWAP_CLUSTER_MAX;
	}

	/* in fact, nvpc pages will never be inside a pvec, so we don't drain */
	// lru_add_drain();
	
	spin_lock_irq(&lruvec->lru_lock);
	nr_taken = nvpc_isolate_evict_pages(nr_to_scan, lruvec, &page_list,
				     &nr_scanned, sc); // NVTODO: isolation for eviction
	__mod_node_page_state(pgdat, NR_ISOLATED_NVPC, nr_taken);
	spin_unlock_irq(&lruvec->lru_lock);

	if (nr_taken == 0)
		return 0;

	nr_reclaimed = shrink_page_list(&page_list, pgdat, sc, false); // NVTODO: evicttion for nvpc

	spin_lock_irq(&lruvec->lru_lock);
	// page_list will contain pages that will not be moved
	nr_taken = nvpc_move_pages_to_lru_reclaim(lruvec, &page_list);
	__mod_node_page_state(pgdat, NR_ISOLATED_NVPC, -nr_taken);
	spin_unlock_irq(&lruvec->lru_lock);

	// NVBUG: free nvpc pages that we won't use here?
	// NVTODO: free_unref_nvpc_page_list(&page_list);
	nvpc_free_pages(&page_list);

	pr_info("[knvpcd nr info evict] nr_reclaimed: %lu, nr_taken: %lu\n", 
			nr_reclaimed, nr_taken);
	return nr_reclaimed;
}



static int do_knvpcd_work(struct nvpc * nvpc, pg_data_t* pgdat)
{
    int ret;

    // Evict pages from NVPC to Disk
	// NVTODO: watermark / trigger
    if (READ_ONCE(nvpc->knvpcd_evict))
    {
	    struct lruvec *target_lruvec;
	    unsigned long nr_to_reclaim = READ_ONCE(nvpc->knvpcd_nr_to_reclaim);
	    unsigned long nr_reclaimed = 0;
	    unsigned long nr_to_scan;
	    struct blk_plug plug;
        struct scan_control sc = {
            .nvpc_evict = 1, // only promotion can be executed
	    };

        nr_to_scan = READ_ONCE(nvpc->knvpcd_nr_to_reclaim); // NVTODO: determine how many pages

	    target_lruvec = mem_cgroup_lruvec(NULL, pgdat);
	    // NVTODO: reclaim should be execute in this block (in the same way as kswapd)
	    
        blk_start_plug(&plug);

	    while (nr_to_scan) {
			unsigned int nr_reclaimed_this_time, nr_to_scan_this_time;
			sc.nr_to_reclaim = nr_to_scan_this_time = min(nr_to_scan, SWAP_CLUSTER_MAX);
		    nr_to_scan -= nr_to_scan_this_time;
            nr_reclaimed_this_time = shrink_nvpc_list(target_lruvec, &sc);
			nr_reclaimed += nr_reclaimed_this_time;

			pr_info("[NVPC DEBUG] nr_evict_this_time: %u\n", nr_reclaimed_this_time);

            cond_resched();

			if (!nr_reclaimed_this_time) // no way to evict more
				break;
            if (nr_reclaimed < nr_to_reclaim)
                continue;
	    }
        blk_finish_plug(&plug);
	    
        if (!ret)
		    pr_warn("[knvpcd_warn] nvpc_evict failed!\n");
    }

    // Demote pages from DRAM to NVPC
    if (READ_ONCE(nvpc->knvpcd_demote))
    {
		// NVTODO: forced demote
		unsigned int order;
		gfp_t gfp_mask;
		struct zone * zone; // for iter 
		struct zoneref * zref; // for iter
		struct zonelist *zonelist;
		enum zone_type highest_zoneidx;

		gfp_mask = GFP_KERNEL | __GFP_KSWAPD_RECLAIM; // NVTODO: review needed
		highest_zoneidx = ZONE_NORMAL; // NVTODO: review needed
				
		zonelist = node_zonelist(NUMA_NO_NODE, gfp_mask); // without any preferences of NUMA selection
		order = order_base_2(READ_ONCE(nvpc->knvpcd_nr_to_promote));

		for_each_zone_zonelist_nodemask(zone, zref, zonelist, highest_zoneidx, NULL)
		{
			wakeup_kswapd(zone, gfp_mask, order, highest_zoneidx);
		}

    }

    // NVTODO: call demotion if DRAM usage reaches watermark
    if (0)
    {
        // NVTODO: demote
    }

    // Promote pages from NVPC to DRAM
    if (READ_ONCE(nvpc->knvpcd_promote))
    {
	    struct lruvec *target_lruvec;
	    unsigned long nr_to_promote = READ_ONCE(nvpc->knvpcd_nr_to_promote);
	    unsigned long nr_reclaimed = 0;
	    unsigned long nr_to_scan;
	    struct blk_plug plug;
        struct scan_control sc = {
            .nvpc_promote = 1, // only promotion can be executed
	    };

        nr_to_scan = READ_ONCE(nvpc->knvpcd_nr_to_promote); // NVTODO: determine how many pages

	    target_lruvec = mem_cgroup_lruvec(NULL, pgdat);
	    // NVTODO: reclaim should be execute in this block (in the same way as kswapd)
	    
        blk_start_plug(&plug);
	    while (nr_to_scan) {
			unsigned int nr_reclaimed_this_time;
		    nr_to_scan -= min(nr_to_scan, SWAP_CLUSTER_MAX);
            nr_reclaimed_this_time = promote_nvpc_list(target_lruvec, &sc);
			nr_reclaimed += nr_reclaimed_this_time;

			pr_info( "[NVPC DEBUG] nr_reclaimed_this_time: %u\n", nr_reclaimed_this_time);

            cond_resched();

			if (!nr_reclaimed_this_time) // no way to promote more
				break;
            if (nr_reclaimed < nr_to_promote)
                continue;
	    }
        blk_finish_plug(&plug);

        if (!ret)
		    pr_warn("[knvpcd_warn] nvpc_promote failed!\n");
    }

    return 0;
}

/**
 * @brief The background pageout daemon, started as a kernel thread
 * from the init process.
 * 
 * @note This basically trickles out pages so that we have _some_
 * free memory available even if there is no other activity
 * that frees anything up. This is needed for things like routing
 * etc, where we otherwise might have all activity going on in
 * asynchronous contexts that cannot page things out.
 * 
 * @note If there are applications that are active memory-allocators
 * (most normal use), this basically shouldn't matter.
 * 
 * @param p 
 * @return int 
 */
static int knvpcd(void *p)
{
	// NVTODO: we need to write back dirty pages in NVPC ***
	pg_data_t *pgdat = (pg_data_t *)p;
	struct task_struct *tsk = current;
	int ret;
	const struct cpumask *cpumask = cpumask_of_node(pgdat->node_id);
	wait_queue_entry_t wait;

	init_waitqueue_entry(&wait, current);

	if (!cpumask_empty(cpumask))
		set_cpus_allowed_ptr(tsk, cpumask);

	/*
	 * Tell the memory management that we're a "memory allocator",
	 * and that if we need more memory we should get access to it
	 * regardless (see "__alloc_pages()"). "knvpcd" should
	 * never get caught in the normal page freeing logic.
	 *
	 * (knvpcd normally doesn't need memory anyway, but sometimes
	 * you need a small amount of memory in order to be able to
	 * page out something else, and this flag essentially protects
	 * us from recursively trying to free more memory as we're
	 * trying to free the first piece of memory in the first place).
	 */
	tsk->flags |= PF_MEMALLOC | PF_SWAPWRITE | PF_KSWAPD;
	set_freezable();

    WRITE_ONCE(nvpc.knvpcd_nr_to_reclaim, SWAP_CLUSTER_MAX);
    WRITE_ONCE(nvpc.knvpcd_nr_to_promote, SWAP_CLUSTER_MAX);

	while (!kthread_should_stop()) {
		nvpc.knvpcd_should_run = 0;
		wait_event_freezable(nvpc.knvpcd_wait, nvpc.knvpcd_should_run);

		WRITE_ONCE(nvpc.knvpcd_nr_to_reclaim, SWAP_CLUSTER_MAX);
		WRITE_ONCE(nvpc.knvpcd_nr_to_promote, SWAP_CLUSTER_MAX);

        ret = do_knvpcd_work(&nvpc, pgdat);

        if (!ret)
            pr_warn("[knvpcd_warn] do nvpc failed!\n");

        // NVTODO: trace mm
		// trace_mm_vmscan_knvpcd_wake(pgdat->node_id, alloc_order);
	}

	tsk->flags &= ~(PF_MEMALLOC | PF_SWAPWRITE | PF_KSWAPD);

	return 0;
}

/*
 * A zone is low on free memory or too fragmented for high-order memory.  If
 * knvpcd should reclaim (direct reclaim is deferred), wake it up for the zone's
 * pgdat.  It will wake up kcompactd after reclaiming memory.  If knvpcd reclaim
 * has failed or is not needed, still wake up kcompactd if only compaction is
 * needed.
 */
void wakeup_knvpcd(int nvpc_promote, int nvpc_demote, int nvpc_evict)
{
    if (!nvpc.enabled)
	{
		return;
	}

	if (!waitqueue_active(&nvpc.knvpcd_wait))
		return;

    if (READ_ONCE(nvpc.knvpcd_promote) < nvpc_promote)
        WRITE_ONCE(nvpc.knvpcd_promote, nvpc_promote);

    if (READ_ONCE(nvpc.knvpcd_demote) < nvpc_demote)
        WRITE_ONCE(nvpc.knvpcd_demote, nvpc_demote);

    if (READ_ONCE(nvpc.knvpcd_evict) < nvpc_evict)
        WRITE_ONCE(nvpc.knvpcd_evict, nvpc_evict);

    // NVTODO: trace mm
	// trace_mm_vmscan_wakeup_knvpcd(0, ZONE_NORMAL, order, gfp_flags);

	WRITE_ONCE(nvpc.knvpcd_should_run, 1); // knvpcd start condition

	wake_up_interruptible(&nvpc.knvpcd_wait);
}
EXPORT_SYMBOL_GPL(wakeup_knvpcd);

/**
 * @brief initialize knvpcd
 * @note rely on system booting to start nvpc
 * 
 * NVTODO: NVPC start on boot
 */
void knvpcd_run(void)
{
	pg_data_t *pgdat = NODE_DATA(0);

	if (nvpc.knvpcd)
		return;

	nvpc.knvpcd = kthread_run(knvpcd, pgdat, "knvpcd%d", 0);
	if (IS_ERR(nvpc.knvpcd)) {
		/* failure at boot is fatal */
		BUG_ON(system_state < SYSTEM_RUNNING);
		pr_err("[knvpcd error] Failed to start knvpcd on node 0\n");
		nvpc.knvpcd = NULL;
	}
}
EXPORT_SYMBOL_GPL(knvpcd_run);


/**
 * @brief stop knvpcd
 * @warning It should never be used, because we don't support multi-node
 * 
 */
void knvpcd_stop(void)
{
	return;
}
EXPORT_SYMBOL_GPL(knvpcd_stop);

void knvpcd_lazy_init(void)
{
	if (!waitqueue_active(&nvpc.knvpcd_wait))
		init_waitqueue_head(&nvpc.knvpcd_wait);
	if (!waitqueue_active(&nvpc.pmemalloc_wait))
    	init_waitqueue_head(&nvpc.pmemalloc_wait);

    knvpcd_run();
}
EXPORT_SYMBOL_GPL(knvpcd_lazy_init);

static int __init knvpcd_init(void)
{
    init_waitqueue_head(&nvpc.knvpcd_wait);
    init_waitqueue_head(&nvpc.pmemalloc_wait);

    knvpcd_run();
	return 0;
}
module_init(knvpcd_init);