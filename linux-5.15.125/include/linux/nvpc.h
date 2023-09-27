#ifndef _LINUX_NVPC_H
#define _LINUX_NVPC_H

#include <linux/dax.h>
#include <linux/uio.h>
#include <linux/list.h>
#include <linux/migrate.h>

#include <linux/nvpc_flag.h>
#include <linux/nvpc_base.h>

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


// void nvpc_lru_size(size_t *free, size_t *total);
// void nvpc_syn_size(size_t *free, size_t *total);
void nvpc_get_usage(size_t *free, size_t *syn_usage, size_t *total);

/* 
 * get a new page when inactive lru migrate to nvpc lru
 * callee hold lru_free_lock
 */
struct page *nvpc_get_new_page(struct page *page, unsigned long private);
void nvpc_free_page(struct page *page, unsigned long private);

/* alloc dram page when nvpc page promote back */
struct page *nvpc_alloc_promote_page(struct page *page, unsigned long node);

#endif