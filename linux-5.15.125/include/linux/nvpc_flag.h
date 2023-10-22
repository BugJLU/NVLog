#ifndef _LINUX_NVPC_FLAG_H
#define _LINUX_NVPC_FLAG_H

/* lru counter */
#define NVPC_LRU_LEVEL_MAX 15
#define NVPC_LRU_LEVEL_SHIFT 4  /* order_base_2(NVPC_LRU_LEVEL_MAX+1) */

#define NVPC_PROMOTE_VEC_SZ 100

#endif