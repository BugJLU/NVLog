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
 * nvpc_move_pages_to_lru() moves pages from private @list to appropriate LRU list.
 * On return, @list is reused as a list of pages to be freed by the caller.
 *
 * Returns the number of pages moved to the given lruvec.
 */
static unsigned int nvpc_move_pages_to_lru(struct lruvec *lruvec,
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

		if (unlikely(!page_evictable(page))) {
			spin_unlock_irq(&lruvec->lru_lock);
			putback_lru_page(page);
			spin_lock_irq(&lruvec->lru_lock);
			continue;
		}

		SetPageLRU(page);

		if (unlikely(put_page_testzero(page))) {
			__clear_page_lru_flags(page);

			if (unlikely(PageCompound(page))) {
				spin_unlock_irq(&lruvec->lru_lock);
				destroy_compound_page(page);
				spin_lock_irq(&lruvec->lru_lock);
			} else
				list_add(&page->lru, &pages_to_free);

			continue;
		}

		/*
		 * All pages were isolated from the same lruvec (and isolation
		 * inhibits memcg migration).
		 */
		VM_BUG_ON_PAGE(!page_matches_lruvec(page, lruvec), page);

		// NVBUG: The page here cannot be NVPC page, because it has been migrated to DRAM
		VM_BUG_ON_PAGE(PageNVPC(page), page);
		add_page_to_lru_list(page, lruvec);

		nr_pages = thp_nr_pages(page);
		nr_moved += nr_pages;
		if (PageActive(page))
			workingset_age_nonresident(lruvec, nr_pages);
	}

	/*
	 * To save our caller's stack, now use input list for pages to free.
	 */
	list_splice(&pages_to_free, list);

	return nr_moved;
}

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

		/* Account the number of base pages even though THP */
		sc->nr_scanned += compound_nr(page);

		VM_BUG_ON_PAGE(!PageNVPC(page), page);
		VM_BUG_ON_PAGE(PageAnon(page), page); // must be file-backed
		VM_BUG_ON_PAGE(PageSwapBacked(page), page); // must be file-backed
		VM_BUG_ON_PAGE(PageTransHuge(page), page); // huge pages not support yet
		VM_BUG_ON_PAGE(PageCompound(page), page); // huge pages not support yet
		VM_BUG_ON_PAGE(page_mapped(page), page); // page should not be mmapped, keep it in DRAM
		VM_BUG_ON_PAGE(!page_is_file_lru(page), page); // page should be in file LRU

		/* NVPC promotion */
		if (do_nvpc_pass && PageNVPC(page) && do_promote_pass) {
			u8 nvpc_lru_cnt = page_nvpc_lru_cnt(page);
			if (get_nvpc()->promote_level && nvpc_lru_cnt >= get_nvpc()->promote_level) {
				list_add(&page->lru, &nvpc_promote_pages);
				unlock_page(page);
				continue;
				// NVTODO: deal with workingset?
			}
		}

keep_locked:
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
			list_splice_init(&nvpc_promote_pages, page_list);
			// do_nvpc_pass = false;
			goto retry; // No other methods can be used
		}
	}

	list_splice(&ret_pages, page_list); // page left unprocessed

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
	nr_taken = nvpc_promote_vec_isolate(&page_list, lruvec); // NVXXX: using united isolation operation
	__mod_node_page_state(pgdat, NR_ISOLATED_NVPC, nr_taken);
	spin_unlock_irq(&lruvec->lru_lock);

	if (nr_taken == 0)
		return 0;

	nr_promoted = promote_page_list(&page_list, pgdat, sc, false);

	spin_lock_irq(&lruvec->lru_lock);
	// page_list will contain pages that will not be moved
	nr_taken = nvpc_move_pages_to_lru(lruvec, &page_list);
	__mod_node_page_state(pgdat, NR_ISOLATED_NVPC, -nr_taken);
	spin_unlock_irq(&lruvec->lru_lock);

	// NVBUG: free nvpc pages that we won't use here?
	free_unref_page_list(&page_list);

	pr_info("[knvpcd nr info promote] nr_promoted: %lu, nr_taken: %lu\n", 
			nr_promoted, nr_taken);
	return nr_promoted;
}










/**
 * Originate from vmscan.c: shrink_page_list()
 * For nvpc eviction use only
 * NVXXX: united page isolation
 */
static unsigned long nvpc_isolate_evict_pages(unsigned long nr_to_scan,
		struct lruvec *lruvec, struct list_head *dst,
		unsigned long *nr_scanned, struct scan_control *sc)
{
	struct list_head *src = &lruvec->lists[LRU_NVPC_FILE]; // NVPC LRU
	unsigned long nr_taken = 0;
	unsigned long skipped = 0;
	unsigned long scan, total_scan, nr_pages;
	LIST_HEAD(pages_skipped);

	total_scan = 0;
	scan = 0;
	while (scan < nr_to_scan && !list_empty(src)) {
		struct list_head *move_to = src;
		struct page *page;

		page = lru_to_page(src);
		// prefetchw_prev_lru_page(page, src, flags); // NVTODO: enable prefetchw on NVDIMMS?

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

static unsigned int shrink_page_list(struct list_head *page_list,
				     struct pglist_data *pgdat,
				     struct scan_control *sc,
				     bool ignore_references)
{
	LIST_HEAD(ret_pages); // return to start as another round
	LIST_HEAD(free_pages); // pages able and need to do eviction (not mark, but operations)

	unsigned int nr_reclaimed = 0;
	// unsigned int pgactivate = 0;

	bool do_nvpc_pass;
	struct nvpc *nvpc = get_nvpc();
	do_nvpc_pass = nvpc->enabled && nvpc->extend_lru;

	cond_resched();

retry:
	while (!list_empty(page_list)) {
		struct address_space *mapping; // XXX: using addr mapping
		struct page *page;
		enum page_references references = PAGEREF_RECLAIM;
		bool dirty, writeback, may_enter_fs;
		unsigned int nr_pages;

		cond_resched();

		page = lru_to_page(page_list);
		list_del(&page->lru);

		if (!trylock_page(page))
		{
			pr_info("[KNVPCD WARN] trylock page failed! Keep it from promotion.\n"); // NVTODO: check whether this is a bug
			goto keep;
		}

		VM_BUG_ON_PAGE(PageActive(page), page);

		nr_pages = compound_nr(page);

		/* Account the number of base pages even though THP */
		sc->nr_scanned += nr_pages;

		if (unlikely(!page_evictable(page)))
			goto activate_locked;

		if (!sc->may_unmap && page_mapped(page))
			goto keep_locked;

		may_enter_fs = (sc->gfp_mask & __GFP_FS) ||
			(PageSwapCache(page) && (sc->gfp_mask & __GFP_IO));


		mapping = page_mapping(page);

		// if (PageWriteback(page)) {
		// 	/* Case 1 above */
		// 	if (current_is_kswapd() &&
		// 	    PageReclaim(page) &&
		// 	    test_bit(PGDAT_WRITEBACK, &pgdat->flags)) {
		// 		stat->nr_immediate++;
		// 		goto activate_locked;

		// 	/* Case 2 above */
		// 	} else if (writeback_throttling_sane(sc) ||
		// 	    !PageReclaim(page) || !may_enter_fs) {
		// 		/*
		// 		 * This is slightly racy - end_page_writeback()
		// 		 * might have just cleared PageReclaim, then
		// 		 * setting PageReclaim here end up interpreted
		// 		 * as PageReadahead - but that does not matter
		// 		 * enough to care.  What we do want is for this
		// 		 * page to have PageReclaim set next time memcg
		// 		 * reclaim reaches the tests above, so it will
		// 		 * then wait_on_page_writeback() to avoid OOM;
		// 		 * and it's also appropriate in global reclaim.
		// 		 */
		// 		SetPageReclaim(page);
		// 		stat->nr_writeback++;
		// 		goto activate_locked;

		// 	/* Case 3 above */
		// 	} else {
		// 		unlock_page(page);
		// 		wait_on_page_writeback(page);
		// 		/* then go back and try same page again */
		// 		list_add_tail(&page->lru, page_list);
		// 		continue;
		// 	}
		// }

		if (!ignore_references)
			references = page_check_references(page, sc);

		switch (references) {
		case PAGEREF_ACTIVATE: // NVTODO: 2nd chance
			goto activate_locked;
		case PAGEREF_KEEP:
			// stat->nr_ref_keep += nr_pages;
			goto keep_locked;
		case PAGEREF_RECLAIM:
		case PAGEREF_RECLAIM_CLEAN:
			; /* try to reclaim the page below */
		}

		/*
		 * Anonymous process memory has backing store?
		 * Try to allocate it some swap space here.
		 * Lazyfree page could be freed directly
		 */
		if (PageAnon(page) && PageSwapBacked(page)) {
			/* nvpc page shouldn't be anon & swapbacked */
			BUG_ON(PageNVPC(page));
// 			if (!PageSwapCache(page)) {
// 				if (!(sc->gfp_mask & __GFP_IO))
// 					goto keep_locked;
// 				if (page_maybe_dma_pinned(page))
// 					goto keep_locked;
// 				if (PageTransHuge(page)) {
// 					/* cannot split THP, skip it */
// 					if (!can_split_huge_page(page, NULL))
// 						goto activate_locked;
// 					/*
// 					 * Split pages without a PMD map right
// 					 * away. Chances are some or all of the
// 					 * tail pages can be freed without IO.
// 					 */
// 					if (!compound_mapcount(page) &&
// 					    split_huge_page_to_list(page,
// 								    page_list))
// 						goto activate_locked;
// 				}
// 				if (!add_to_swap(page)) {
// 					if (!PageTransHuge(page))
// 						goto activate_locked_split;
// 					/* Fallback to swap normal pages */
// 					if (split_huge_page_to_list(page,
// 								    page_list))
// 						goto activate_locked;
// #ifdef CONFIG_TRANSPARENT_HUGEPAGE
// 					count_vm_event(THP_SWPOUT_FALLBACK);
// #endif
// 					if (!add_to_swap(page))
// 						goto activate_locked_split;
// 				}

// 				may_enter_fs = true;

// 				/* Adding to swap updated mapping */
// 				mapping = page_mapping(page);
// 			}
		} else if (unlikely(PageTransHuge(page))) {
			/* nvpc page shouldn't be THP */
			BUG_ON(PageNVPC(page));
			/* Split file THP */
			// if (split_huge_page_to_list(page, page_list))
			// 	goto keep_locked;
		}

		/*
		 * THP may get split above, need minus tail pages and update
		 * nr_pages to avoid accounting tail pages twice.
		 *
		 * The tail pages that are added into swap cache successfully
		 * reach here.
		 */
		if ((nr_pages > 1) && !PageTransHuge(page)) {
			/* nvpc page shouldn't be thp */
			BUG_ON(PageNVPC(page));
			// sc->nr_scanned -= (nr_pages - 1);
			// nr_pages = 1;
		}

		/*
		 * The page is mapped into the page tables of one or more
		 * processes. Try to unmap it here.
		 */
		if (page_mapped(page) && !PageNVPC(page)) {
			enum ttu_flags flags = TTU_BATCH_FLUSH;
			bool was_swapbacked = PageSwapBacked(page);

			if (unlikely(PageTransHuge(page))) {
				BUG_ON(PageNVPC(page));
				flags |= TTU_SPLIT_HUGE_PMD;
			}

			try_to_unmap(page, flags);
			if (page_mapped(page)) {
				stat->nr_unmap_fail += nr_pages;
				if (!was_swapbacked && PageSwapBacked(page))
					stat->nr_lazyfree_fail += nr_pages;
				goto activate_locked;
			}
		}

		// if (PageDirty(page)) {
		// 	/*
		// 	 * Only kswapd can writeback filesystem pages
		// 	 * to avoid risk of stack overflow. But avoid
		// 	 * injecting inefficient single-page IO into
		// 	 * flusher writeback as much as possible: only
		// 	 * write pages when we've encountered many
		// 	 * dirty pages, and when we've already scanned
		// 	 * the rest of the LRU for clean pages and see
		// 	 * the same dirty pages again (PageReclaim).
		// 	 */
		// 	if (page_is_file_lru(page) &&
		// 	    (!current_is_kswapd() || !PageReclaim(page) ||
		// 	     !test_bit(PGDAT_DIRTY, &pgdat->flags))) {
		// 		/*
		// 		 * Immediately reclaim when written back.
		// 		 * Similar in principal to deactivate_page()
		// 		 * except we already have the page isolated
		// 		 * and know it's dirty
		// 		 */
		// 		inc_node_page_state(page, NR_VMSCAN_IMMEDIATE);
		// 		SetPageReclaim(page);

		// 		goto activate_locked;
		// 	}

		// 	if (references == PAGEREF_RECLAIM_CLEAN)
		// 		goto keep_locked;
		// 	if (!may_enter_fs)
		// 		goto keep_locked;
		// 	if (!sc->may_writepage)
		// 		goto keep_locked;

		// 	/*
		// 	 * Page is dirty. Flush the TLB if a writable entry
		// 	 * potentially exists to avoid CPU writes after IO
		// 	 * starts and then write it out here.
		// 	 */
		// 	try_to_unmap_flush_dirty();
		// 	switch (pageout(page, mapping)) {
		// 	case PAGE_KEEP:
		// 		goto keep_locked;
		// 	case PAGE_ACTIVATE:
		// 		goto activate_locked;
		// 	case PAGE_SUCCESS:
		// 		stat->nr_pageout += thp_nr_pages(page);

		// 		if (PageWriteback(page))
		// 			goto keep;
		// 		if (PageDirty(page))
		// 			goto keep;

		// 		/*
		// 		 * A synchronous write - probably a ramdisk.  Go
		// 		 * ahead and try to reclaim the page.
		// 		 */
		// 		if (!trylock_page(page))
		// 			goto keep;
		// 		if (PageDirty(page) || PageWriteback(page))
		// 			goto keep_locked;
		// 		mapping = page_mapping(page);
		// 		fallthrough;
		// 	case PAGE_CLEAN:
		// 		; /* try to free the page below */
		// 	}
		// }

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
		//  NVTODO: test this block
		if (page_has_private(page)) {
			if (!try_to_release_page(page, sc->gfp_mask))
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

		if (PageAnon(page) && !PageSwapBacked(page)) {
			BUG_ON(PageNVPC(page));
			// /* follow __remove_mapping for reference */
			// if (!page_ref_freeze(page, 1))
			// 	goto keep_locked;
			// /*
			//  * The page has only one reference left, which is
			//  * from the isolation. After the caller puts the
			//  * page back on lru and drops the reference, the
			//  * page will be freed anyway. It doesn't matter
			//  * which lru it goes. So we don't bother checking
			//  * PageDirty here.
			//  */
			// count_vm_event(PGLAZYFREED);
			// count_memcg_page_event(page, PGLAZYFREED);
		} else if (!mapping || !__remove_mapping(mapping, page, true,
							 sc->target_mem_cgroup))
			goto keep_locked;

		unlock_page(page);
free_it:
		/*
		 * THP may get swapped out in a whole, need account
		 * all base pages.
		 */
		nr_reclaimed += nr_pages;

		/*
		 * Is there need to periodically free_page_list? It would
		 * appear not as the counts should be low
		 */
		if (unlikely(PageTransHuge(page))) {
			WARN_ON(PageNVPC(page))
			destroy_compound_page(page);
		}
#ifdef CONFIG_NVPC
		else if(PageNVPC(page))
			list_add(&page->lru, &free_nvpc_pages);
#endif
		else
			list_add(&page->lru, &free_pages);
		continue;

activate_locked_split:
		/*
		 * The tail pages that are failed to add into swap cache
		 * reach here.  Fixup nr_scanned and nr_pages.
		 */
		if (nr_pages > 1) {
			sc->nr_scanned -= (nr_pages - 1);
			nr_pages = 1;
		}
activate_locked:
		if (PageNVPC(page))
			goto keep_locked;
		
		// /* Not a candidate for swapping, so reclaim swap space. */
		// if (PageSwapCache(page) && (mem_cgroup_swap_full(page) ||
		// 				PageMlocked(page)))
		// 	try_to_free_swap(page);
		// VM_BUG_ON_PAGE(PageActive(page), page);
		// if (!PageMlocked(page)) {
		// 	int type = page_is_file_lru(page);
		// 	SetPageActive(page);
		// 	stat->nr_activate[type] += nr_pages;
		// 	count_memcg_page_event(page, PGACTIVATE);
		// }
keep_locked:
		unlock_page(page);
keep:
		list_add(&page->lru, &ret_pages);
		VM_BUG_ON_PAGE(PageLRU(page) || PageUnevictable(page), page);
	}
	/* 'page_list' is always empty here */

	pgactivate = stat->nr_activate[0] + stat->nr_activate[1];

#ifdef CONFIG_NVPC
	nvpc_free_pages(&free_nvpc_pages);
#endif

	BUG_ON(!list_empty(&free_pages)); // NVTODO: no nvpc page in free_pages

	// mem_cgroup_uncharge_list(&free_pages);
	try_to_unmap_flush();
	// free_unref_page_list(&free_pages);

	list_splice(&ret_pages, page_list);
	// count_vm_events(PGACTIVATE, pgactivate);

	return nr_reclaimed;
}


// Originate from vmscan.c: shrink_inactive_list()
// Include evict and writeback
// NVTODO: second chance should be added by PG_active flag
static unsigned long
shrink_nvpc_list(struct lruvec *lruvec, struct scan_control *sc)
{
	LIST_HEAD(page_list);
	unsigned long nr_reclaimed;
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
	nr_taken = nvpc_isolate_evict_pages(nr_to_scan, lruvec, &page_list,
				     &nr_scanned, sc, lru); // NVTODO: isolation for eviction
	__mod_node_page_state(pgdat, NR_ISOLATED_NVPC, nr_taken);
	spin_unlock_irq(&lruvec->lru_lock);

	if (nr_taken == 0)
		return 0;

	nr_reclaimed = evict_page_list(&page_list, pgdat, sc, false); // NVTODO: evicttion for nvpc

	spin_lock_irq(&lruvec->lru_lock);
	// page_list will contain pages that will not be moved
	nr_taken = nvpc_move_pages_to_lru(lruvec, &page_list);
	__mod_node_page_state(pgdat, NR_ISOLATED_NVPC, -nr_taken);
	spin_unlock_irq(&lruvec->lru_lock);

	// NVBUG: free nvpc pages that we won't use here?
	free_unref_page_list(&page_list);

	pr_info("[knvpcd nr info evict] nr_reclaimed: %lu, nr_taken: %lu\n", 
			nr_reclaimed, nr_taken);
	return nr_reclaimed;
}






static int do_knvpcd_work(struct nvpc * nvpc, pg_data_t* pgdat)
{
    int ret;

    // NVTODO: Evict pages from NVPC to Disk
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
			unsigned int nr_reclaimed_this_time;
		    nr_to_scan -= min(nr_to_scan, SWAP_CLUSTER_MAX);
            nr_reclaimed_this_time = promote_nvpc_list(target_lruvec, &sc);
			nr_reclaimed += nr_reclaimed_this_time;

			printk(KERN_WARNING "[NVPC DEBUG] nr_evict_this_time: %u\n", nr_reclaimed_this_time);

            cond_resched();

			if (!nr_reclaimed_this_time) // no way to evict more
				break;

            if (nr_reclaimed < nr_to_promote)
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

			printk(KERN_WARNING "[NVPC DEBUG] nr_reclaimed_this_time: %u\n", nr_reclaimed_this_time);

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


/*
 * The background pageout daemon, started as a kernel thread
 * from the init process.
 *
 * This basically trickles out pages so that we have _some_
 * free memory available even if there is no other activity
 * that frees anything up. This is needed for things like routing
 * etc, where we otherwise might have all activity going on in
 * asynchronous contexts that cannot page things out.
 *
 * If there are applications that are active memory-allocators
 * (most normal use), this basically shouldn't matter.
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

        // NVTODO: undone trace mm
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

    // NVTODO: undone
	// trace_mm_vmscan_wakeup_knvpcd(0, ZONE_NORMAL, order, gfp_flags);

	WRITE_ONCE(nvpc.knvpcd_should_run, 1); // knvpcd start condition

	wake_up_interruptible(&nvpc.knvpcd_wait);
}
EXPORT_SYMBOL_GPL(wakeup_knvpcd);


/** 
 * NVTODO: NVPC start on boot
 * 
 * rely on system booting to start nvpc
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
 * NVTODO: It should never be used, because we don't support multi-node
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