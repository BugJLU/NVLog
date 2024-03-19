#ifndef _LINUX_NVPC_FLAG_H
#define _LINUX_NVPC_FLAG_H

/* lru counter */
#define NVPC_LRU_LEVEL_MAX 15
#define NVPC_LRU_LEVEL_SHIFT 4  /* order_base_2(NVPC_LRU_LEVEL_MAX+1) */

#define NVPC_PROMOTE_VEC_SZ 100

#define NVPC_DEMOTE_REFAULT

/* 
 * NVPC_EVICT_WATERMARK: percentage of PMEM memory as a warning water level
 * NVPC_EVICT_PERCENT: the percentage of pages should be evicted once
 * 
 * if the remaining PMEM is less then NVPC_EVICT_WATERMARK% of NVPC
 * then we should evict NVPC_EVICT_PERCENT% pages from NVPC
 */
#define NVPC_EVICT_WATERMARK    2
#define NVPC_EVICT_PERCENT      2

#define NVPC_PERCPU_FREELIST

#define NVPC_IPOOP_THR PAGE_SIZE

#define NVPC_ACTIVE_SYNC_SENSITVT   2
// #define NVPC_ACTIVE_SYNC_THRESH     100 // deprecated
#define NVPC_ACTIVE_SYNC_LAT_NVM    1   // NVTODO: evaluate this
#define NVPC_ACTIVE_SYNC_LAT_NVM1   4096  // NVTODO: evaluate this

// #define NVPC_TRANS_ON
// #define NVPC_COMPACT_DAEMON_ON
// #define NVPC_LIGHT_TRANS
#define NVPC_ACTIVE_SYNC_ON

#endif