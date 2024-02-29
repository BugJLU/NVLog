#include <linux/nvpc_rw.h>

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
    // NVTODO: evaluate the performance between this and nvpc_write_nv_iter()
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
    if (unlikely(off + len > (nvpc.len_pg << PAGE_SHIFT)))
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
    if (unlikely(off + len > (nvpc.len_pg << PAGE_SHIFT)))
    {
        len = (nvpc.len_pg << PAGE_SHIFT) - off;
    }
    pr_debug("Libnvpc: prepared read len %zu\n", len);
    len = _nvpc_read_nv_iter(to, off, len);
    pr_debug("Libnvpc: actual read len %zu\n", len);

    return len;
}