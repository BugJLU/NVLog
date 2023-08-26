#include <linux/types.h>
#include <linux/dax.h>
#include <linux/uio.h>

struct nvpc
{
    bool enabled;
    struct dax_device *dax_dev;
    /* kernel address that the dax_dev is mapped to */
    void *dax_kaddr;
    /* mapped length in pages */
    size_t len_pg;

    
    // TODO: we may need a lock for the struct
};

void init_nvpc(struct dax_device *dev);
void fini_nvpc(void);
struct nvpc *get_nvpc(void);

/* get the address at an offset of nvpc */
void *nvpc_get_addr(loff_t off);

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