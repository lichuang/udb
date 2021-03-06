#ifndef _UDB_PAGE_CACHE_H_
#define _UDB_PAGE_CACHE_H_

#include <udb.h>

#include "page.h"
#include "types.h"

/*
** The cache_item_base_t object represents a single page in the
** page cache.  The page cache will allocate instances of this
** object.  Various methods of the page cache use pointers to instances
** of this object as parameters or as their return value.
**
** See [cache_methods_t] for additional information.
*/
struct cache_item_base_t {
  void *buf;   /* The content of the page */
  void *extra; /* Extra information associated with the page */
};

struct cache_config_t {
  int pageSize;
  int extraSize;
  int (*Stress)(void *, page_t *); /* Call to try make a page clean */
  void *stressArg;                 /* Argument to Stress */
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

/* Create a new page item flag. */
#define CACHE_CREATE_FLAG_CREATE                                               \
  (CACHE_CREATE_FLAG_EASY_ALLOCATE & CACHE_CREATE_FLAG_HARD_ALLOCATE)

struct cache_methods_t {
  int version;
  void *arg;

  udb_code_t (*Init)(void *);
  void (*Shutdown)(void *);

  cache_module_t *(*Create)(int, int);
  void (*CacheSize)(cache_module_t *, int nCacheSize);
  int (*PageCount)(cache_module_t *);
  cache_item_base_t *(*Fetch)(cache_module_t *, page_no_t key,
                              cache_create_flag_t flag);
  void (*Unpin)(cache_module_t *, cache_item_base_t *, bool);
  void (*Destroy)(cache_module_t *);
};

/* Initialize and shutdown the page cache subsystem */
udb_code_t cache_init();
void cache_shutdown();

/*
** pre-allocate memory for cache buffer with
** udb_config_t.preAllocatePageCacheNum.
*/
udb_code_t cache_setup_buffer(int, int);

/* Create a new pager cache.
** Under memory stress, invoke Stress to try to make pages clean.
** Only clean and unpinned pages can be reclaimed.
*/
udb_code_t cache_open(cache_config_t *, page_cache_t *);

/* Reset and close the cache object */
void cache_close(page_cache_t *);

/* Modify the page size after the cache has been created. */
udb_code_t cache_set_page_size(page_cache_t *, int);

cache_item_base_t *cache_fetch(page_cache_t *, page_no_t, cache_create_flag_t);
udb_code_t cache_fetch_stress(page_cache_t *, page_no_t, page_t **);
page_t *cache_fetch_finish(page_cache_t *, page_no_t, cache_item_base_t *);
void cache_release_page(page_t *);

void cache_drop(page_t *);
void cache_mark_dirty(page_t *);
void cache_mark_clean(page_t *);
void cache_clean_all(page_cache_t *);

/* Return the total number of pages stored in the cache */
int cache_page_count(page_cache_t *);

void cache_use_default_methods();

#endif /* _UDB_PAGE_CACHE_H_ */