#ifndef _LINUX_NVPC_RW_H
#define _LINUX_NVPC_RW_H

#include <linux/dax.h>
#include <linux/uio.h>
#include <linux/nvpc_base.h>

/* copy data from kernel to nvpc */
void nvpc_write_nv(void *from, loff_t off, size_t len);
/* copy data from nvpc to kernel */
void nvpc_read_nv(void *to, loff_t off, size_t len);

/* copy data from user to nvpc */
size_t nvpc_write_nv_iter(struct iov_iter *from, loff_t off, bool flush);
// size_t nvpc_write_nv_iter_noflush(struct iov_iter *from, loff_t off, size_t len);
/* copy data from nvpc to user */
size_t nvpc_read_nv_iter(struct iov_iter *to, loff_t off);

#define nvpc_write_commit pmem_wmb

#endif