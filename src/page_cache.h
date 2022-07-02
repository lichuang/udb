#ifndef _UDB_PAGE_CACHE_H_
#define _UDB_PAGE_CACHE_H_

#include <udb.h>

#include "types.h"

struct cache_item_t {
  void *buf;   /* The content of the page */
  void *extra; /* Extra information associated with the page */
};

/*
** Whether or not a new page may be allocated if the page not found.
** 0 means do not allocate a new page.
** 1 means allocate a new page if space is easily available.
** 2 means to try really hard to allocate a new page.
*/
typedef enum cache_create_flag_t {
  CACHE_CREATE_FLAG_DONOT_CREATE = 0,
  CACHE_CREATE_FLAG_EASY_ALLOCATE = 1,
  CACHE_CREATE_FLAG_HARD_ALLOCATE = 2,
} cache_create_flag_t;

struct cache_methods_t {
  int version;
  void *arg;
  int (*Init)(void *);
  void (*Shutdown)(void *);

  cache_item_t *(*Fetch)(cache_methods_t *, page_id_t key,
                         cache_create_flag_t flag);
};

extern cache_methods_t *default_cache_methods;

/*
** A page is pinned if it is not on the LRU list.  To be "pinned" means
** that the page is in active use and must not be deallocated.
*/
#define ITEM_IS_PINNED(p) ((p)->lru_next == NULL)
#define ITEM_IS_UNPINNED(p) ((p)->lru_next != NULL)

udb_err_t cache_open(page_cache_t **, cache_methods_t *methods);
void cache_close(page_cache_t *);

page_t *cache_fetch(page_cache_t *, page_id_t, cache_create_flag_t);
udb_err_t cache_fetch_stress(page_cache_t *, page_id_t, page_t **);
page_t *cache_fetch_finish(page_cache_t *, page_id_t, page_t *);
void cache_drop(page_cache_t *, page_t *);

#endif /* _UDB_PAGE_CACHE_H_ */