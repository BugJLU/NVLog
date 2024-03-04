#ifndef _LINUX_NVPC_H
#define _LINUX_NVPC_H

#include <linux/dax.h>
#include <linux/uio.h>
#include <linux/list.h>
#include <linux/migrate.h>
#include <linux/sched.h>

#include <linux/nvpc_flag.h>
#include <linux/nvpc_base.h>
#include <linux/nvpc_rw.h>

// #define CONFIG_NVPC true // NVTODO: remove this when debug is done

struct nvpc_opts
{
    struct dax_device *dev;
    int nid;        /* numa node id */
    bool extend_lru;
    bool absorb_syn;
    bool nvpc_lru_evict;
    u8 promote_level;
    bool demote_before_promote;
    size_t nvpc_sz;  /* in pages */
    // size_t syn_sz;  /* in pages */
    bool force; // force to initialize
    bool rebuild; // rebuild the nvpc data in NVM
};

// Initialize and finalize the NVPC subsystem
int init_nvpc(struct nvpc_opts *opts);
void fini_nvpc(void);

/* get the nvpc struct */
static inline struct nvpc *get_nvpc(void)
{
    // if nvpc is per-node, we should return the one in the local node
    return &nvpc;
}

/* get the address at an offset of nvpc */
static inline void *nvpc_get_addr(loff_t off)
{
    return get_nvpc()->dax_kaddr + off;
}

/* get the address at a page offset of nvpc */
static inline void *nvpc_get_addr_pg(loff_t off_pg)
{
    return get_nvpc()->dax_kaddr + (off_pg << PAGE_SHIFT);
}

static inline loff_t nvpc_get_off(void *kaddr)
{
    return kaddr - get_nvpc()->dax_kaddr;
}

static inline loff_t nvpc_get_off_pg(void *kaddr)
{
    return (((uintptr_t)kaddr & PAGE_MASK) - (uintptr_t)get_nvpc()->dax_kaddr) >> PAGE_SHIFT;
}

static inline int current_is_knvpcd(void)
{
    return (current->flags & PF_KSWAPD) && (current == get_nvpc()->knvpcd);
}

static inline bool do_nvpc(void)
{
    return get_nvpc()->enabled && get_nvpc()->extend_lru;
}

void nvpc_get_usage(size_t *free, size_t *syn_usage, size_t *total);

/* 
 * get a new page when inactive lru migrate to nvpc lru
 * callee hold lru_free_lock
 */
struct page *nvpc_get_new_page(struct page *page, unsigned long private);
size_t nvpc_get_n_new_page(struct list_head *pages, size_t n);
void nvpc_free_page(struct page *page, unsigned long private);
void nvpc_free_lru_page(struct page *page);
void nvpc_free_pages(struct list_head *list);

/* alloc dram page when nvpc page promote back */
struct page *nvpc_alloc_promote_page(struct page *page, unsigned long node);

int nvpc_promote_vec_put_page(struct page * page);
void nvpc_promote_vec_clear(void);
// bool nvpc_should_promote(void);
int nvpc_promote_vec_nr(void);

/* knvpcd */
extern void knvpcd_run(void);
extern void knvpcd_stop(void);
void wakeup_knvpcd(unsigned long nr_nvpc_promote, unsigned long nr_nvpc_evict);
extern void knvpcd_lazy_init(void);
void nvpc_wakeup_nvpc_promote(void);
void nvpc_wakeup_nvpc_evict(void);


// NVTODO: for debug, remove these
extern int debug_print;
extern int debug_ino;
#define nv_pr_info(fmt, ...) \
	debug_print?printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__):1==1;

#define nv_dump_stack() debug_print?dump_stack():1==1;

static inline void set_debug_print_on(void)
{
    debug_print=1;
}
static inline void set_debug_print_off(void)
{
    debug_print=0;
}

#endif