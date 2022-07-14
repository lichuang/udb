#include <assert.h>
#include <stdio.h>

#include "global.h"
#include "memory/alloc.h"
#include "page.h"
#include "pagecache/page_cache.h"

/*
** A complete page cache is an instance of this structure.  Every
** entry in the cache holds a single page of the database file.  The
** btree layer only operates on the cached copy of the database pages.
**
** A page cache entry is "clean" if it exactly matches what is currently
** on disk.  A page is "dirty" if it has been modified and needs to be
** persisted to disk.
**
** pDirty, pDirtyTail, pSynced:
**   All dirty pages are linked into the doubly linked list using
**   PgHdr.pDirtyNext and pDirtyPrev. The list is maintained in LRU order
**   such that p was added to the list more recently than p->pDirtyNext.
**   PCache.pDirty points to the first (newest) element in the list and
**   pDirtyTail to the last (oldest).
**
**   The PCache.pSynced variable is used to optimize searching for a dirty
**   page to eject from the cache mid-transaction. It is better to eject
**   a page that does not require a journal sync than one that does.
**   Therefore, pSynced is maintained so that it *almost* always points
**   to either the oldest page in the pDirty/pDirtyTail list that has a
**   clear PGHDR_NEED_SYNC flag or to a page that is older than this one
**   (so that the right page to eject can be found by following pDirtyPrev
**   pointers).
*/
struct page_cache_t {
  page_t *dirty, *dirtyTail;       /* List of dirty pages in LRU order */
  page_t *synced;                  /* Last synced page in dirty page list */
  int refSum;                      /* Sum of ref counts over all pages */
  int cacheSize;                   /* Configured cache size */
  int spillSize;                   /* Size before spilling occurs */
  int pageSize;                    /* Size of every page in this cache */
  int extraSize;                   /* Size of extra space for each page */
  cache_create_flag_t createFlag;  /* createFlag value for for Fetch() */
  int (*Stress)(void *, page_t *); /* Call to try make a page clean */
  void *stressArg;                 /* Argument to Stress */
  cache_module_t *cacheModule;     /* Pluggable cache module */
};

/* Static internal function forward declarations */

/* Outer function implementations */

/*
** Create a new page_cache_t object. Storage space to hold the object
** has already been allocated and is passed in as the p pointer.
** The caller discovers how much space needs to be allocated by
** calling sqlite3PcacheSize().
**
** szExtra is some extra space allocated for each page.  The first
** 8 bytes of the extra space will be zeroed as the page is allocated,
** but remaining content will be uninitialized.  Though it is opaque
** to this module, the extra space really ends up being the MemPage
** structure in the pager.
*/
udb_err_t cache_open(cache_config_t *config, page_cache_t *cache) {
  memset(cache, 0, sizeof(page_cache_t));
  cache->extraSize = config->extraSize;
  cache->createFlag = CACHE_CREATE_FLAG_HARD_ALLOCATE;
  cache->Stress = config->Stress;
  cache->stressArg = config->stressArg;
  cache->cacheSize = 100;
  cache->spillSize = 1;

  return cache_set_page_size(cache, config->pageSize);
}

void cache_close(page_cache_t *cache) { udb_free(cache); }

/*
** Change the page size for page_cache_t object. The caller must ensure that
** there are no outstanding page references when this function is called.
*/
udb_err_t cache_set_page_size(page_cache_t *cache, int pageSize) {
  assert(cache->refSum == 0 && cache->dirty == NULL);

  cache_module_t *cacheModule;
  cache_methods_t *methods = udbGlobalConfig.cacheMethods;

  cacheModule =
      methods->Create(pageSize, cache->extraSize + ROUND8(sizeof(page_t)));

  if (cacheModule == NULL) {
    return UDB_OOM;
  }
  if (cache->cacheModule) {
    methods->Destroy(cache->cacheModule);
  }
  cache->cacheModule = cacheModule;
  cache->pageSize = pageSize;

  return UDB_OK;
}

/*
 ** fetch an item from cache by the page id.
 */
page_t *cache_fetch(page_cache_t *cache, page_id_t id,
                    cache_create_flag_t create_flag) {
  cache_item_base_t *item = udbGlobalConfig.cacheMethods->Fetch(
      udbGlobalConfig.cacheMethods->arg, id, create_flag);

  return NULL;
}
