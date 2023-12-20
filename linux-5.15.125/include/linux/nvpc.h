#ifndef _LINUX_NVPC_H
#define _LINUX_NVPC_H

#include <linux/dax.h>
#include <linux/uio.h>
#include <linux/list.h>
#include <linux/migrate.h>

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
    size_t nvpc_sz;  /* in pages */
    // size_t syn_sz;  /* in pages */
    u8 promote_level;
};

int init_nvpc(struct nvpc_opts *opts);
void fini_nvpc(void);
static inline struct nvpc *get_nvpc(void)
{
    // if nvpc is per-node, we should return the one in the local node
    return &nvpc;
}

/* get the address at an offset of nvpc */
static inline void *nvpc_get_addr(loff_t off)
{
    return nvpc.dax_kaddr + off;
}

/* get the address at a page offset of nvpc */
static inline void *nvpc_get_addr_pg(loff_t off_pg)
{
    return nvpc.dax_kaddr + (off_pg << PAGE_SHIFT);
}

void nvpc_get_usage(size_t *free, size_t *syn_usage, size_t *total);

/* 
 * get a new page when inactive lru migrate to nvpc lru
 * callee hold lru_free_lock
 */
struct page *nvpc_get_new_page(struct page *page, unsigned long private);
void nvpc_free_page(struct page *page, unsigned long private);
void nvpc_free_lru_page(struct page *page);
void nvpc_free_pages(struct list_head *list);

/* alloc dram page when nvpc page promote back */
struct page *nvpc_alloc_promote_page(struct page *page, unsigned long node);

int nvpc_promote_vec_put_page(struct page * page);
void nvpc_promote_vec_clear(void);
int nvpc_promote_vec_isolate(struct list_head *page_list);
// bool nvpc_should_promote(void);
int nvpc_promote_vec_nr(void);

/* knvpcd */
extern void knvpcd_run(void);
extern void knvpcd_stop(void);
extern void wakeup_knvpcd(int nvpc_promote, int nvpc_demote, int nvpc_evict);
extern void knvpcd_lazy_init(void);

// NVTODO: for debug, remove these
extern int debug_print;
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