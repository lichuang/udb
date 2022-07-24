#include <assert.h>
#include <stdio.h>

#include "global.h"
#include "macros.h"
#include "memory/memory.h"
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
  page_t *dirty, *dirtyTail;      /* List of dirty pages in LRU order */
  page_t *synced;                 /* Last synced page in dirty page list */
  int refSum;                     /* Sum of ref counts over all pages */
  int cacheSize;                  /* Configured cache size */
  int spillSize;                  /* Size before spilling occurs */
  int pageSize;                   /* Size of every page in this cache */
  int extraSize;                  /* Size of extra space for each page */
  cache_create_flag_t createFlag; /* createFlag value for for Fetch() */
  udb_code_t (*Stress)(void *, page_t *); /* Call to try make a page clean */
  void *stressArg;                        /* Argument to Stress */
  cache_module_t *cacheModule;            /* Pluggable cache module */
};

typedef enum MANAGE_DIRTY_LIST_FLAG {
  MANAGE_DIRTY_LIST_FLAG_REMOVE = 1,
  MANAGE_DIRTY_LIST_FLAG_ADD = 2,
  MANAGE_DIRTY_LIST_FLAG_FRONT = 3,
} MANAGE_DIRTY_LIST_FLAG;

/* Static internal function forward declarations */
static page_t *__fetch_finish_with_init(page_cache_t *, page_no_t,
                                        cache_item_base_t *);
static void __unpin_page(page_t *);
static void __manage_dirty_list(page_t *, MANAGE_DIRTY_LIST_FLAG);

#ifdef UDB_DEBUG
static bool __page_sanity(page_t *);
#endif

/* Static internal function implementations */

static page_t *__fetch_finish_with_init(page_cache_t *cache, page_no_t no,
                                        cache_item_base_t *base) {
  page_t *page = NULL;
  assert(base != NULL);

  page = (page_t *)base->extra;
  assert(page->base == NULL);
  memset(&page->dirty, 0, sizeof(page_t) - offsetof(page_t, dirty));
  page->base = base;
  page->data = base->buf;
  page->extra = (void *)&page[1];
  /*
   ** The first 8 bytes of the extra space will be zeroed as the page is
   ** allocated
   */
  memset(&page->extra, 0, 8);
  page->cache = cache;
  page->id = id;
  page->flags = PAGE_FLAG_CLEAN;

  return cache_fetch_finish(page, id, base);
}

/*
** Wrapper around the pluggable caches Unpin method.
*/
static void __unpin_page(page_t *page) {
  udbGlobalConfig.cacheMethods->Unpin(page->cache, page, false);
}

/*
** Manage page's participation on the dirty list.  Bits of the addRemove
** argument determines what operation to do.  The 0x01 bit means first
** remove page from the dirty list.  The 0x02 means add page back to
** the dirty list.  Doing both moves page to the front of the dirty list.
*/
static void __manage_dirty_list(page_t *page, MANAGE_DIRTY_LIST_FLAG flags) {
  page_cache_t *cache = page->cache;

  if (flags & MANAGE_DIRTY_LIST_FLAG_REMOVE) {
    assert(page->dirtyNext || page == cache->dirtyTail);
    assert(page->dirtyPrev || page == cache->dirty);

    /* Update the page_cache_t.synced variable if necessary. */
    if (page->dirtyNext) {
      page->dirtyNext->dirtyPrev = page->dirtyPrev;
    } else {
      assert(page == cache->dirtyTail);
      cache->dirtyTail = page->dirtyPrev;
    }

    if (page->dirtyPrev) {
      page->dirtyPrev->dirtyNext = page->dirtyNext;
    } else {
      /* If there are now no dirty pages in the cache, set createFlag to
       ** CACHE_CREATE_FLAG_HARD_ALLOCATE.
       ** This is an optimization that allows cache_fetch() to skip
       ** searching for a dirty page to eject from the cache when it might
       ** otherwise have to.  */
      asssert(page == cache->dirty);
      cache->dirty = page->dirtyNext;
      if (cache->dirty == NULL) {
        cache->createFlag = CACHE_CREATE_FLAG_HARD_ALLOCATE;
      }
    }
  }

  if (flags & MANAGE_DIRTY_LIST_FLAG_ADD) {
    page->dirtyPrev = NULL;
    page->dirtyNext = cache->dirty;
    if (page->dirtyNext) {
      assert(page->dirtyNext->dirtyPrev == NULL);
      page->dirtyNext->dirtyPrev = page;
    } else {
      cache->dirtyTail = page;
      assert(cache->createFlag == CACHE_CREATE_FLAG_HARD_ALLOCATE);
      cache->createFlag = CACHE_CREATE_FLAG_EASY_ALLOCATE;
    }
  }
  cache->dirty = page;

  /* If synced is NULL, set
   ** pSynced to point to it. */
  if (cache->synced == NULL) {
    cache->synced = page;
  }
}

#ifdef UDB_DEBUG
static bool __page_sanity(page_t *page) {}
#endif

/* Outer function implementations */

extern void cache_use_default_methods();

udb_code_t cache_init() {
  if (udbGlobalConfig.cacheMethods->Init == NULL) {
    cache_use_default_methods();
    assert(udbGlobalConfig.cacheMethods->Init != NULL);
  }

  return udbGlobalConfig.cacheMethods->Init(udbGlobalConfig.cacheMethods->arg);
}

void cache_shutdown() {
  if (udbGlobalConfig.cacheMethods->Shutdown == NULL) {
    udbGlobalConfig.cacheMethods->Shutdown(udbGlobalConfig.cacheMethods->arg);
  }
}

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
udb_code_t cache_open(cache_config_t *config, page_cache_t *cache) {
  memset(cache, 0, sizeof(page_cache_t));
  cache->extraSize = config->extraSize;
  cache->createFlag = CACHE_CREATE_FLAG_HARD_ALLOCATE;
  cache->Stress = config->Stress;
  cache->stressArg = config->stressArg;
  cache->cacheSize = 100;
  cache->spillSize = 1;

  return cache_set_page_size(cache, config->pageSize);
}

void cache_close(page_cache_t *cache) {
  assert(cache->cacheModule != NULL);
  udbGlobalConfig.cacheMethods->Destroy(cache->cacheModule);
}

/*
** Change the page size for page_cache_t object. The caller must ensure that
** there are no outstanding page references when this function is called.
*/
udb_code_t cache_set_page_size(page_cache_t *cache, int pageSize) {
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
cache_item_base_t *cache_fetch(page_cache_t *cache, page_no_t no,
                               cache_create_flag_t createFlag) {
  cache_create_flag_t newCreateFlag;
  cache_item_base_t *base;

  assert(cache != NULL);
  assert(cache->cacheModule != NULL);
  assert(createFlag == CACHE_CREATE_FLAG_CREATE ||
         createFlag == CACHE_CREATE_FLAG_DONOT_CREATE);
  assert(cache->createFlag == cache->dirty ? CACHE_CREATE_FLAG_EASY_ALLOCATE
                                           : CACHE_CREATE_FLAG_HARD_ALLOCATE);

  newCreateFlag = createFlag & cache->createFlag;
  assert(newCreateFlag == CACHE_CREATE_FLAG_DONOT_CREATE ||
         newCreateFlag == CACHE_CREATE_FLAG_EASY_ALLOCATE ||
         newCreateFlag == CACHE_CREATE_FLAG_HARD_ALLOCATE);
  assert(createFlag == CACHE_CREATE_FLAG_DONOT_CREATE ||
         cache->createFlag == newCreateFlag);

  return udbGlobalConfig.cacheMethods->Fetch(udbGlobalConfig.cacheMethods->arg,
                                             id, newCreateFlag);
}

/*
** If the cache_fetch() routine is unable to allocate a new
** page because no clean pages are available for reuse and the cache
** size limit has been reached, then this routine can be invoked to
** try harder to allocate a page.  This routine might invoke the stress
** callback to spill dirty pages to the journal.  It will then try to
** allocate the new page and will only fail to allocate a new page on
** an OOM error.
**
** This routine should be invoked only after cache_fetch() fails.
*/
udb_code_t cache_fetch_stress(page_cache_t *cache, page_no_t no,
                              page_t **page) {
  page_t *p;
  udb_code_t err = UDB_OK;

  *page = NULL;

  if (cache->createFlag == CACHE_CREATE_FLAG_HARD_ALLOCATE) {
    return UDB_OK;
  }

  if (cache_page_count(cache) > cache->spillSize) {
    /* Find a dirty page to write-out and recycle. First try to find a
    ** page that does not require a journal-sync (one with PGHDR_NEED_SYNC
    ** cleared), but if that is not possible settle for any other
    ** unreferenced dirty page.
    **
    ** If the LRU page in the dirty list that has a clear PGHDR_NEED_SYNC
    ** flag is currently referenced, then the following may leave pSynced
    ** set incorrectly (pointing to other than the LRU page with NEED_SYNC
    ** cleared). This is Ok, as pSynced is just an optimization.  */
    for (p = cache->synced; p && p->refNum > 0; p = p->dirtyPrev)
      ;
    cache->synced = p;
    if (!p) {
      for (p = cache->dirtyTail; p && p->refNum > 0; p = p->dirtyNext)
        ;
    }
    if (p) {
      err = cache->Stress(cache->stressArg, p);
      if (err != UDB_OK && err != UDB_BUSY) {
        return err;
      }
    }
  }

  *page = udbGlobalConfig.cacheMethods->Fetch(cache->cacheModule, id,
                                              CACHE_CREATE_FLAG_HARD_ALLOCATE);

  return *page == NULL ? UDB_OOM : UDB_OK;
}

/*
** This routine converts the cache_item_base_t object returned by
** cache_fetch() into an initialized PgHdr object.  This routine
** must be called after cache_fetch() in order to get a usable
** result.
*/
page_t *cache_fetch_finish(page_cache_t *cache, page_no_t no,
                           cache_item_base_t *base) {
  page_t *page = NULL;

  assert(base != NULL);

  page = base->extra;
  if (!page->base) {
    return __fetch_finish_with_init(cache, id, base);
  }
  cache->refSum++;
  page->refNum++;

#ifdef UDB_DEBUG
  assert(__page_sanity(page));
#endif
  return page;
}

/*
** Decrement the reference count on a page. If the page is clean and the
** reference count drops to 0, then it is made eligible for recycling.
*/
void cache_release_page(page_t *page) {
  assert(page->refNum > 0);
  page->cache->refSum -= 1;
  if (--page->refNum == 0) {
    if (page->flags & PAGE_FLAG_CLEAN) {
      __unpin_page(page);
    } else {
      __manage_dirty_list(page, MANAGE_DIRTY_LIST_FLAG_FRONT);
    }
  }
}

/*
** Drop a page from the cache. There must be exactly one reference to the
** page. This function deletes that reference, so after it returns the
** page pointed to by p is invalid.
*/
void cache_drop(page_t *page) {
  assert(page->refNum == 1);
#ifdef UDB_DEBUG
  assert(__page_sanity(page));
#endif

  if (page->flags & PAGE_FLAG_DIRTY) {
    __manage_dirty_list(page, MANAGE_DIRTY_LIST_FLAG_REMOVE);
  }
  page->cache->refSum--;
  udbGlobalConfig.cacheMethods->Unpin(page->cache->cacheModule, page->base,
                                      true);
}

/*
** Make sure the page is marked as dirty. If it isn't dirty already,
** make it so.
*/
void cache_mark_dirty(page_t *page) {
  assert(page->refNum > 0);
#ifdef UDB_DEBUG
  assert(__page_sanity(page));
#endif

  if (!(page->flags & (PAGE_FLAG_CLEAN | PAGE_FLAG_DONT_WRITE))) {
    return;
  }
  page->flags &= ~PAGE_FLAG_DONT_WRITE;
  if (page->flags & PAGE_FLAG_CLEAN) {
    page->flags ^= (PAGE_FLAG_DIRTY | PAGE_FLAG_CLEAN);
    assert((page->flags & (PAGE_FLAG_DIRTY | PAGE_FLAG_CLEAN)) ==
           PAGE_FLAG_DIRTY);
    __manage_dirty_list(page, MANAGE_DIRTY_LIST_FLAG_ADD);
  }
#ifdef UDB_DEBUG
  assert(__page_sanity(page));
#endif
}

/*
** Make sure the page is marked as clean. If it isn't clean already,
** make it so.
*/
void cache_mark_clean(page_t *page) {
#ifdef UDB_DEBUG
  assert(__page_sanity(page));
#endif

  assert((page->flags & PAGE_FLAG_DIRTY) != 0);
  assert((page->flags & PAGE_FLAG_CLEAN) == 0);

  __manage_dirty_list(page, MANAGE_DIRTY_LIST_FLAG_REMOVE);
  page->flags &= ~(PAGE_FLAG_DIRTY);
  page->flags |= (PAGE_FLAG_CLEAN);
#ifdef UDB_DEBUG
  assert(__page_sanity(page));
#endif
  if (page->refNum == 0) {
    __unpin_page(page);
  }
}

/*
** Make every page in the cache clean.
*/
void cache_clean_all(page_cache_t *cache) {
  pager_t *page;
  while ((page = cache->dirty) != NULL) {
    cache_mark_clean(page);
  }
}

/*
** Return the total number of pages in the cache.
*/
int cache_page_count(page_cache_t *cache) {
  assert(cache->cacheModule != NULL);
  return udbGlobalConfig.cacheMethods->PageCount(cache->cacheModule);
}