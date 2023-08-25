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

// TODO: remove DEBUG
#define DEBUG

#include <linux/nvpc.h>
#include <linux/uio.h>
#include <linux/kernel.h>

/* NVPC is global, use get_nvpc to get the pointer to this instance */
static struct nvpc nvpc = {
    .enabled = false
};

static bool support_clwb;

void init_nvpc(struct dax_device *dev)
{
    nvpc.dax_dev = dev;

    // XXX: maybe we don't really need to map the whole device into kaddr?
    nvpc.len_pg = dax_map_whole_dev(dev, &nvpc.dax_kaddr);

    /*
     * TODO: DAXDEV_OCCUPIED bit is set in dax_device->flags to 
     * indicate that this device is occupied by nvpc. However, 
     * we haven't change any other code to check this bit before 
     * they use a dax device. And we may also need a lock.
     * 
     * update 2023/8/22: maybe we can remove this since we use 
     * blkdev_get_by_path and blkdev_put with FMODE_EXCL flag
     */
    set_dax_occupied(dev);

    support_clwb = static_cpu_has(X86_FEATURE_CLWB);
    nvpc.enabled = true;
}

void fini_nvpc(void)
{
    nvpc.enabled = false;
}

struct nvpc *get_nvpc(void)
{
    return &nvpc;
}

void *nvpc_get_addr(loff_t off) {
    return nvpc.dax_kaddr + off;
}

/* copy data to nvpc */
void nvpc_write_nv(void *from, loff_t off, size_t len)
{
    /* movnt for bulk, clwb for residue on x86*/
    memcpy_flushcache(nvpc.dax_kaddr+off, from, len);
}

/* copy data from nvpc */
void nvpc_read_nv(void *to, loff_t off, size_t len)
{
    memcpy(to, nvpc.dax_kaddr+off, len);
}

/* 
 * return number of bytes copied 
 */
static inline size_t _nvpc_write_nv_iter(struct iov_iter *from, loff_t off, size_t len)
{
    // dax_copy_from_iter(nvpc.dax_dev, 0, nvpc.dax_kaddr + off, len, from);
    /* movnt for bulk, clwb for residue on x86*/
    return _copy_from_iter_flushcache(nvpc.dax_kaddr + off, len, from);
}

/* 
 * copy data from iov to nvpc, without cache flush
 * return number of bytes copied
 */
static inline size_t _nvpc_write_nv_iter_noflush(struct iov_iter *from, loff_t off, size_t len)
{
    // TODO: evaluate the performance between this and nvpc_write_nv_iter()
    /*
     * In NVPC sometimes we don't care about the persistency of the data, like 
     * when we are dealing with the lru. This function can be used to provide a
     * higher speed...?
     */
    return _copy_from_iter(nvpc.dax_kaddr + off, len, from);
}

/* 
 * return number of bytes copied
 */
static inline size_t _nvpc_read_nv_iter(struct iov_iter *to, loff_t off, size_t len)
{
    // dax_copy_to_iter(nvpc.dax_dev, 0, nvpc.dax_kaddr + off, len, to);
    return _copy_mc_to_iter(nvpc.dax_kaddr + off, len, to);
}

/*
 * copy data from iov to nvpc
 * if flush == true, cache will be flushed after copy
 * return number of bytes copied
 */
size_t nvpc_write_nv_iter(struct iov_iter *from, loff_t off, bool flush)
{
    size_t len;

    len = iov_iter_count(from);
    if (off + len > (nvpc.len_pg << PAGE_SHIFT))
    {
        len = (nvpc.len_pg << PAGE_SHIFT) - off;
    }
    
    pr_debug("Libnvpc: prepared write len %zu\n", len);
    if (flush)
        len = _nvpc_write_nv_iter(from, off, len);
    else
        len = _nvpc_write_nv_iter_noflush(from, off, len);
    pr_debug("Libnvpc: actual write len %zu\n", len);
    
    return len;
}

/* 
 * copy data from nvpc to iov 
 * return number of bytes copied
 */
size_t nvpc_read_nv_iter(struct iov_iter *to, loff_t off)
{
    size_t len;

    len = iov_iter_count(to);
    if (off + len > (nvpc.len_pg << PAGE_SHIFT))
    {
        len = (nvpc.len_pg << PAGE_SHIFT) - off;
    }
    pr_debug("Libnvpc: prepared read len %zu\n", len);
    len = _nvpc_read_nv_iter(to, off, len);
    pr_debug("Libnvpc: actual read len %zu\n", len);

    return len;
}

void nvpc_wmb(void) {
    pmem_wmb();
}