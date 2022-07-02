#ifndef _UDB_TYPES_H_
#define _UDB_TYPES_H_

#include <stdint.h>

/* Forward declarations of all the internal types. */

typedef struct mutex_t mutex_t;

typedef uint32_t page_id_t;

typedef uint64_t offset_t;

typedef struct pager_t pager_t;

typedef struct page_t page_t;

typedef struct page_cache_t page_cache_t;
typedef struct cache_item_t cache_item_t;
typedef struct cache_methods_t cache_methods_t;

typedef struct node_t node_t;

typedef struct file_t file_t;

typedef uint32_t wal_frame_t;
typedef struct wal_t wal_t;

#endif /* _UDB_TYPES_H_ */