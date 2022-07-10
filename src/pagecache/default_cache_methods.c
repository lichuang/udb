#include <assert.h>
#include <stdio.h>

#include "fault.h"
#include "global.h"
#include "macros.h"
#include "memory/alloc.h"
#include "os/mutex.h"
#include "pagecache/page_cache.h"

typedef struct default_cache_t default_cache_t;
typedef struct default_cache_group_t default_cache_group_t;
typedef struct default_cache_item_t default_cache_item_t;
typedef struct page_free_slot_t page_free_slot_t;

struct default_cache_item_t {
  cache_item_t base;      /* Base class, Must be first. */
  page_id_t key;          /* Key value (page id) */
  default_cache_t *cache; /* Cache that currently owns this item */
  bool isAnchor;          /* This is the default_cache_group_t.lru element */
  bool isBulkLocal;       /* This item from bulk local storage */
  default_cache_item_t *next;    /* Next item in hash table chain */
  default_cache_item_t *lruNext; /* Next page in LRU list of unpined items */
  default_cache_item_t *lruPrev; /* Prev page in LRU list of unpined items */
};

/*
** Each page cache (or default_cache_t) belongs to a default_cache_group_t.
** A default_cache_group_t is a set
** of one or more default_cache_t that are able to recycle each other's unpinned
** pages when they are under memory pressure.  A default_cache_group_t is an
** instance of the following object.
**
** This page cache implementation works in one of two modes:
**
**   (1)  Every default_cache_t is the sole member of its own
**        default_cache_group_t.  There is one default_cache_group_t per
**        default_cache_t.
**
**   (2)  There is a single global default_cache_group_t that all
**        default_cache_t are a member of.
**
** Mode 1 uses more memory (since PCache instances are not able to rob
** unused pages from other PCaches) but it also operates without a mutex,
** and is therefore often faster.  Mode 2 requires a mutex in order to be
** threadsafe, but recycles pages more efficiently.
**
** For mode (1), PGroup.mutex is NULL.  For mode (2) there is only a single
** PGroup which is the pcache1.grp global variable and its mutex is
** SQLITE_MUTEX_STATIC_LRU.
*/
struct default_cache_group_t {
  mutex_t *mutex;
  uint32_t maxItemNum;      /* Sum of nMax for purgeable caches */
  uint32_t minItemNum;      /* Sum of nMin for purgeable caches */
  uint32_t maxPinnedItem;   /* nMaxpage + 10 - nMinPage */
  uint32_t purgeableItem;   /* Number of purgeable pages allocated */
  default_cache_item_t lru; /* The beginning and end of the LRU list */
};

/*
** Each page cache is an instance of the following object.  Every
** open database file (including each in-memory database and each
** temporary or transient database) has a single page cache which
** is an instance of this object.
**
** Pointers to structures of this type are saved in the cache_methods_t arg
*  field.
*/
struct default_cache_t {
  default_cache_group_t *group; /* Group this cache belongs to */
  int pageSize;                 /* Size of database page */
  int extraSize;                /* sizeof(node_t)+sizeof(page_t) */
  int itemSize;                 /* Total size of one cache item */
  uint32_t minItemNum;          /* The Minimum number of items reserved */
  uint32_t maxItemNum;          /* Configured "cache_size" value */
  uint32_t max90Percent;        /* maxItemNum*9/10 */
  page_id_t maxKey;             /* Largest key seen since Truncate() */

  /* Hash table of all pages. The following variables may only be accessed
  ** when the accessor is holding the default_cache_group_t mutex.
  */
  uint32_t recyclable;         /* Number of pages in the LRU list */
  uint32_t itemNum;            /* Total number of items in hash */
  uint32_t slotNum;            /* Number of slots in hash[] */
  default_cache_item_t **hash; /* Hash table for lookup by key */
  default_cache_item_t *free;  /* List of unused pcache-local pages */
};

/*
** Global data used by default cache.
*/
static struct default_cache_global_t {
  default_cache_group_t group; /* The global cache group for mode (2) */

  /* Variables related to SQLITE_CONFIG_PAGECACHE settings.  The
  ** szSlot, nSlot, pStart, pEnd, nReserve, and isInit values are all
  ** fixed at sqlite3_initialize() time and do not require mutex protection.
  ** The nFreeSlot and pFree values do require mutex protection.
  */
  int inited;        /* True if initialized */
  int separateCache; /* Use a new page group for each page cache */
  int initItemNum;   /* Initial bulk allocation size */
  int slotSize;      /* Size of each free slot */
  int slotNum;       /* The number of pcache slots */
  int reserved;      /* Try to keep nFreeSlot above this */
  void *start, *end; /* Bounds of global page cache memory */
  /* Above requires no mutex.  Use mutex below for variable that follow. */
  mutex_t *mutex;             /* Mutex for accessing the following: */
  page_free_slot_t *freeSlot; /* Free page blocks */
  int freeSlot;               /* Number of unused pcache slots */
  /* The following value requires a mutex to change.  We skip the mutex on
  ** reading because (1) most platforms read a 32-bit integer atomically and
  ** (2) even if an incorrect value is read, no great harm is done since this
  ** is really just an optimization. */
  int underPressure; /* True if low on PAGECACHE memory */
} defaultCacheGlobal;

static const uint32_t MIN_HASH_SLOT_NUM = 256;

/*
** Macros to enter and leave the default Cache LRU mutex.
*/
#if !defined(SQLITE_ENABLE_MEMORY_MANAGEMENT) || SQLITE_THREADSAFE == 0
#define defaultCacheEnterMutex(X) assert((X)->mutex == 0)
#define defaultCacheLeaveMutex(X) assert((X)->mutex == 0)
#define DEFAULT_CACHE_MIGHT_USE_GROUP_MUTEX 0
#else
#define defaultCacheEnterMutex(X) mutex_enter((X)->mutex)
#define defaultCacheLeaveMutex(X) mutex_leave((X)->mutex)
#define DEFAULT_CACHE_MIGHT_USE_GROUP_MUTEX 1
#endif

/* The default cache method forward declarations */
udb_err_t default_cache_init(void *);
void default_cache_shutdown(void *);
cache_module_t *default_cache_create(cache_module_config_t *);
void default_cache_destroy(cache_module_t *);
static cache_item_t *default_cache_fetch(cache_module_t *arg, page_id_t key,
                                         cache_create_flag_t flag);

/* Static internal function forward declarations */

static void calc_group_max_pinned_item_num(default_cache_group_t *);
static bool is_cache_under_memory_pressure(default_cache_t *cache);
static bool is_cache_nearly_full(default_cache_t *cache);

static void resize_hash(default_cache_t *);
static default_cache_item_t *try_recycle_item(default_cache_t *cache);
static void remove_from_hash(default_cache_item_t *, bool free_item);
static default_cache_item_t *alloc_item(default_cache_t *, int benignMalloc);
static void free_item(default_cache_item_t *);
static default_cache_item_t *alloc_item(default_cache_t *);
static default_cache_item_t *init_item(default_cache_t *, page_id_t,
                                       default_cache_item_t *);
static bool init_bulk(default_cache_t *);

static default_cache_item_t *pin_item(default_cache_item_t *);

static default_cache_item_t *fetch_no_mutex(default_cache_t *, page_id_t key,
                                            cache_create_flag_t flag);

static default_cache_item_t *fetch_stage2(default_cache_t *cache, page_id_t key,
                                          cache_create_flag_t flags);

static void *cache_alloc_buffer(int);
static void cache_free_buffer(void *);
static void cache_truncate_unsafe(default_cache_t *, page_id_t);
static void cache_resize_hash(default_cache_t *cache);

/* Static function implementations */

static inline void
calc_group_max_pinned_item_num(default_cache_group_t *group) {
  group->maxPinnedItem = group->maxItemNum + 10 - group->minItemNum;
}

static bool is_cache_under_memory_pressure(default_cache_t *cache) {
  return false;
}

static bool is_cache_nearly_full(default_cache_t *cache) {
  uint32_t pinned_num;
  default_cache_group_t *group = cache->group;

  assert(cache->itemNum >= cache->recyclable);

  pinned_num = cache->itemNum - cache->recyclable;
  return ((pinned_num >= group->maxPinnedItem ||
           pinned_num >= cache->max90Percent) ||
          (is_cache_under_memory_pressure(cache) &&
           cache->recyclable < pinned_num));
}

/*
** Malloc function used within this file to allocate space from the buffer
** configured using sqlite3_config(SQLITE_CONFIG_PAGECACHE) option. If no
** such buffer exists or there is no space left in it, this function falls
** back to sqlite3Malloc().
**
** Multiple threads can run this routine at the same time.  Global variables
** in pcache1 need to be protected via mutex.
*/
static void *cache_alloc_buffer(int) {}

/*
** Free an allocated buffer obtained from cache_alloc_buffer().
*/
static void cache_free_buffer(void *p) {}

/*
** Discard all pages from cache with a page number (key value)
** greater than or equal to iLimit. Any pinned pages that meet this
** criteria are unpinned before they are discarded.
**
** The default_cache_t mutex must be held when this function is called.
*/
static void cache_truncate_unsafe(default_cache_t *cache, page_id_t limit) {
  uint32_t h, stop;

  assert(mutex_held(cache->group->mutex));
  assert(cache->maxKey >= limit);
  assert(cache->slotNum > 0);

  if (cache->maxKey - limit < cache->slotNum) {
    /* If we are just shaving the last few pages off the end of the
     ** cache, then there is no point in scanning the entire hash table.
     ** Only scan those hash slots that might contain pages that need to
     ** be removed. */
    h - limit % cache->slotNum;
    stop = cache->maxKey - cache->slotNum;
  } else {
    /* This is the general case where many pages are being removed.
     ** It is necessary to scan the entire hash table */
    h = cache->slotNum / 2;
    stop = h - 1;
  }

  while (true) {
    default_cache_item_t **pp;
    default_cache_item_t *item;
    pp = &cache->hash[h];
    /* free the item which key >= limit */
    while ((item = *pp) != NULL) {
      if (item->key >= limit) {
        cache->itemNum -= 1;
        pp = &item->next;
        if (ITEM_IS_UNPINNED(item)) {
          pin_item(item);
        }
        free_item(item);
      } else {
        pp = &item->next;
      }
    }
    if (h == stop) {
      break;
    }
    h = (h + 1) % cache->slotNum;
  }
}

/*
** This function is used to resize the hash table used by the cache passed
** as the first argument.
**
** The default_cache_t mutex must be held when this function is called.
*/
static void cache_resize_hash(default_cache_t *cache) {
  default_cache_item_t **newHash;
  uint32_t newSlotNum;
  uint32_t i;

  assert(mutex_held(cache->group->mutex));

  newSlotNum = cache->slotNum * 2;
  if (newSlotNum < MIN_HASH_SLOT_NUM) {
    newSlotNum = MIN_HASH_SLOT_NUM;
  }

  defaultCacheLeaveMutex(cache->group);
  if (cache->hash != NULL) {
    faultBeginBenignMalloc();
  }

  newHash = udb_calloc(sizeof(default_cache_item_t **) * newSlotNum);
  if (newHash == NULL) {
    return;
  }

  faultEndBenignMalloc();

  defaultCacheEnterMutex(cache->group);
  for (i = 0; i < cache->slotNum; i++) {
    default_cache_item_t *item;
    default_cache_item_t *nextItem = cache->hash[i];
    while ((item = newItem) != NULL) {
      uint32_t h = item->key % newSlotNum;
      nextItem = item->next;
      item->next = newHash[h];
      newHash[h] = item;
    }
  }
  udb_free(cache->hash);
  cache->hash = newHash;
  cache->slotNum = newSlotNum;
}

static default_cache_item_t *try_recycle_item(default_cache_t *cache) {
  default_cache_group_t *group = cache->group;
  default_cache_t *other;
  default_cache_item_t *item = NULL;

  if (group->lru.lruPrev->isAnchor) {
    return NULL;
  }

  if (cache->itemNum + 1 < cache->maxItemNum &&
      is_cache_under_memory_pressure(cache)) {
    return NULL;
  }

  item = group->lru.lruPrev;
  assert(ITEM_IS_UNPINNED(item));
  remove_from_hash(item, false);
  pin_item(item);
  other = item->cache;
  if (other->itemSize != cache->itemSize) {
    free_item(item);
    item = NULL;
  } else {
    group->purgeableItem -= (other->purgeable - cache->purgeable);
  }

  return item;
}

/*
** Remove the page supplied as an argument from the hash table
** (default_cache_t.hash structure) that it is currently stored in.
** Also free the page if default_cache_t is true.
**
** The default_cache_group_t mutex must be held when this function is called.
*/
static void remove_from_hash(default_cache_item_t *item, bool free_flag) {
  default_cache_t *cache = item->cache;
  uint32_t h;
  default_cache_item_t **prev_item;

  assert(mutex_held(cache->group->mutex));
  h = item->key % cache->slotNum;
  for (prev_item = &cache->hash[h]; (*prev_item) != item;
       prev_item = &(*prev_item)->next)
    ;
  *prev_item = (*prev_item)->next;

  cache->itemNum -= 1;
  if (free_flag) {
    free_item(item);
  }
}

static default_cache_item_t *alloc_item(default_cache_t *, int benignMalloc) {}

/*
** Free a item allocated by alloc_item().
*/
static void free_item(default_cache_item_t *item) {
  assert(item != NULL);
  default_cache_t *cache = item->cache;
  assert(mutex_held(cache->group->mutex));
  if (item->isBulkLocal) {
    item->next = cache->next;
    cache->free = item;
  } else {
    cache_free_buffer(item->base.buf);
  }
}

/*
** Allocate a new item object initially associated with cache.
*/
static default_cache_item_t *alloc_item(default_cache_t *cache) {
  default_cache_item_t *item;
  void *data;

  assert(mutex_held(cache->group->mutex));

  if (cache->free || (cache->itemNum == 0 && init_bulk(cache))) {
    assert(cache->free != NULL);
    /* free the item from free list */
    item = cache->free;
    cache->free = item->next;
    item->next = NULL;
  } else {
    data = udb_alloc(cache->itemSize);
    if (data == NULL) {
      return NULL;
    }
    item = (default_cache_item_t *)&((uint8_t *)data)[cache->pageSize];
    item->base.buf = data;
    item->base.extraSize = &item[1];
    item->isBulkLocal = false;
    item->isAnchor = false;
    item->lruPrev = NULL;
  }
  cache->group->purgeableItem += 1;

  return item;
}

static default_cache_item_t *init_item(default_cache_t *cache, page_id_t key,
                                       default_cache_item_t *item) {
  assert(cache != NULL);
  assert(item != NULL);

  uint32_t h = key % cache->slotNum;
  cache->itemNum += 1;
  item->key = key;
  item->next = cache->hash[h];
  item->cache = cache;
  item->lruNext = NULL;
  *(void **)item->base.extraSize = NULL;
  cache->hash[h] = item;

  return item;
}

/*
** Try to initialize the cache->free and pCache->pBulk fields.
** Return true if cache->free ends up containing one or more free pages.
*/
static bool init_bulk(default_cache_t *cache) {
  int64_t bulk_size;
  int bulk_num;
  char *bulk;
  default_cache_item_t *item;

  if (defaultCacheGlobal.initItemNum == 0) {
    return false;
  }

  /* Do not bother with a bulk allocation if the cache size very small */
  if (cache->maxItemNum < 3) {
    return false;
  }

  if (defaultCacheGlobal.initItemNum > 0) {
    bulk_size = cache->itemSize * defaultCacheGlobal.initItemNum;
  } else {
    bulk_size = -1024 * defaultCacheGlobal.initItemNum;
  }
  if (bulk_size > cache->itemSize * cache->maxItemNum) {
    bulk_size = cache->itemSize * cache->maxItemNum;
  }

  bulk = udb_alloc(bulk_size);
  if (bulk == NULL) {
    return false;
  }

  /* init the bulk cache items */
  bulk_num = bulk_size / cache->itemSize;
  do {
    item = (default_cache_item_t *)&bulk[cache->pageSize];
    item->base.extraSize = &item[1];
    item->isAnchor = false;
    item->isBulkLocal = true;
    item->next = cache->free;
    item->lruPrev = NULL;
    cache->free = item;
    bulk += cache->itemSize;
  } while (--bulk_num);

  return cache->free != NULL;
}

static default_cache_item_t *fetch_no_mutex(default_cache_t *cache,
                                            page_id_t key,
                                            cache_create_flag_t flag) {
  default_cache_item_t *item = NULL;

  assert(cache != NULL);

  /* Step 1: Search the hash table for an existing entry. */
  item = cache->hash[key % cache->slotNum];
  while (item && item->key != key) {
    item = item->next;
  }

  /* Step 2: If the page was found in the hash table, then return it. */
  if (item) {
    if (ITEM_IS_UNPINNED(item)) {
      return pin_item(item);
    }
    return item;
  }

  /* If the page was not in the hash table and create_flags is 0, abort. */
  if (flag == CACHE_CREATE_FLAG_DONOT_CREATE) {
    return NULL;
  }

  /*
  ** Otherwise (page not in hash and create_flags!=0) continue with
  ** subsequent steps to try to create the page.
  */
  return fetch_stage2(cache, key, flag);
}

static default_cache_item_t *fetch_stage2(default_cache_t *cache, page_id_t key,
                                          cache_create_flag_t flag) {
  default_cache_group_t *group = cache->group;
  default_cache_item_t *item;

  /* Step 3: Abort if createFlag is 1 but the cache is nearly full */
  if (flag == CACHE_CREATE_FLAG_EASY_ALLOCATE && is_cache_nearly_full(cache)) {
    return NULL;
  }

  if (cache->itemNum >= cache->slotNum) {
    cache_resize_hash(cache);
  }
  assert(cache->slotNum > 0 && cache->hash != NULL);

  /* Step 4. Try to recycle an item. */
  item = try_recycle_item(cache);

  /*
  ** Step 5. If a usable page buffer has still not been found,
  ** attempt to allocate a new one.
  */
  if (item == NULL) {
    item = alloc_item(cache);
  }

  if (item == NULL) {
    return NULL;
  }

  return init_item(cache, key, item);
}

/*
** This function is used internally to remove the item from the
** default_cache_group_t LRU list, if is part of it. If item is not part of the
** group LRU list, then this function is a no-op.
**
** The default_cache_group_t mutex must be held when this function is called.
*/
static default_cache_item_t *pin_item(default_cache_item_t *item) {
  assert(item != NULL);
  assert(ITEM_IS_UNPINNED(item));
  assert(item->lruNext);
  assert(item->lruPrev);
  aseert(mutex_held(item->cache->group->mutex));

  /* Remove the item from the LRU list */
  item->lruNext->lruPrev = item->lruPrev;
  item->lruPrev->lruNext = item->lruNext;

  item->lruNext = item->lruPrev = NULL;

  assert(!item->isAnchor);
  assert(item->cache->group->lru.isAnchor);

  item->cache->recyclable -= 1;

  return item;
}

/* The default cache method implementations */

/*
** Implementation of the default_cache_methods.Init method.
*/
udb_err_t default_cache_init(void *notUsed) {
  UNUSED_PARAMETER(notUsed);

  assert(!defaultCacheGlobal.inited);

  memset(&defaultCacheGlobal, 0, sizeof(defaultCacheGlobal));

  /*
  ** The pcache1.separateCache variable is true if each PCache has its own
  ** private PGroup (mode-1).  pcache1.separateCache is false if the single
  ** PGroup in pcache1.grp is used for all page caches (mode-2).
  **
  **   *  Always use a unified cache (mode-2) if ENABLE_MEMORY_MANAGEMENT
  **
  **   *  Use a unified cache in single-threaded applications that have
  **      configured a start-time buffer for use as page-cache memory using
  **      sqlite3_config(SQLITE_CONFIG_PAGECACHE, pBuf, sz, N) with non-NULL
  **      pBuf argument.
  **
  **   *  Otherwise use separate caches (mode-1)
  */
#if defined(SQLITE_ENABLE_MEMORY_MANAGEMENT)
  defaultCacheGlobal.separateCache = 0;
#elif SQLITE_THREADSAFE
  defaultCacheGlobal.separateCache =
      sqlite3GlobalConfig.page == NULL || sqlite3GlobalConfig.bCoreMutex > 0;
#else
  defaultCacheGlobal.separateCache = udbGlobalConfig.page == NULL;
#endif

#if SQLITE_THREADSAFE
  if (sqlite3GlobalConfig.bCoreMutex) {
    defaultCacheGlobal.group.mutex = sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_LRU);
    defaultCacheGlobal.mutex = sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_PMEM);
  }
#endif
  if (defaultCacheGlobal.separateCache && udbGlobalConfig.pageNum != 0 &&
      udbGlobalConfig.page == NULL) {
    defaultCacheGlobal.initItemNum = udbGlobalConfig.pageNum;
  } else {
    defaultCacheGlobal.initItemNum = 0;
  }
  defaultCacheGlobal.group.maxPinnedItem = 10;
  defaultCacheGlobal.inited = true;

  return UDB_OK;
}

/*
** Implementation of the default_cache_methods.Shutdown method.
*/
void default_cache_shutdown(void *notUsed) {
  UNUSED_PARAMETER(notUsed);
  assert(defaultCacheGlobal.inited);
  memset(&defaultCacheGlobal, 0, sizeof(defaultCacheGlobal));
}

/*
** Implementation of the default_cache_methods.Create method.
** Allocate a new cache.
*/
cache_module_t *default_cache_create(cache_module_config_t *config) {
  default_cache_t *cache;     /* The newly created page cache */
  default_cache_group_t *grp; /* The group the new page cache will belong to */
  int sz; /* Bytes of memory required to allocate the new cache */
  bool separateCache = defaultCacheGlobal.separateCache;

  assert(config != NULL);
  assert(VALID_PAGE_SIZE(config->pageSize));
  assert(config->extraSize < 300);

  sz = sizeof(default_cache_t) + sizeof(default_cache_group_t) * separateCache;

  cache = (default_cache_t *)udb_calloc(sz);
  if (cache == NULL) {
    return NULL;
  }
  if (separateCache) {
    grp = (default_cache_group_t *)&cache[1];
  } else {
    grp = &defaultCacheGlobal.group;
  }
  defaultCacheEnterMutex(grp);
  if (!grp->lru.isAnchor) {
    grp->lru.isAnchor = true;
    grp->lru.lruPrev = grp->lru.lruNext = &grp->lru;
  }
  cache->group = grp;
  cache->pageSize = config->pageSize;
  cache->extraSize = config->extraSize;
  cache->itemSize = config->pageSize + config->extraSize +
                    ROUND8(sizeof(default_cache_item_t));
  cache_resize_hash(cache);

  cache->minItemNum = 10;
  grp->minItemNum += cache->minItemNum;
  calc_group_max_pinned_item_num(grp);

  defaultCacheLeaveMutex(grp);

  if (cache->hash == NULL) {
    default_cache_destroy(cache);
    cache = NULL;
  }
  return (cache_module_t *)cache;
}

/*
** Implementation of the default_cache_methods.Destroy method.
** Destroy a cache allocated using default_cache_create().
*/
void default_cache_destroy(cache_module_t *p) {
  default_cache_t *cache = (default_cache_t *)p;
  default_cache_group_t *grp = cache->group;

  defaultCacheEnterMutex(grp);
  if (cache->itemNum > 0) {
    cache_truncate_unsafe(cache, 0);
  }
  assert(grp->maxItemNum >= cache->maxItemNum);
  grp->maxItemNum -= cache->maxItemNum;
  assert(grp->minItemNum >= cache->minItemNum);
  grp->minItemNum -= cache->minItemNum;
  calc_group_max_pinned_item_num(grp);
  defaultCacheLeaveMutex(grp);
  udb_free(cache->bulk);
  udb_free(cache->hash);
  udb_free(cache);
}

/*
** Implementation of the default_cache_methods.Fetch method.
*/
static cache_item_t *default_cache_fetch(cache_module_t *arg, page_id_t key,
                                         cache_create_flag_t flag) {
  /*
    return (cache_item_t *)fetch_no_mutex((default_cache_t *)method->arg, key,
                                          flag);
                                          */
}

struct cache_methods_t default_cache_methods = {
    1,                      /* version */
    NULL,                   /* arg */
    default_cache_init,     /* Init */
    default_cache_shutdown, /* Shutdown */
    default_cache_fetch,    /* Fetch */
};

void cache_use_default_methods() {
  static const cache_methods_t default_cache_methods = {
      1,                      /* version */
      NULL,                   /* arg */
      default_cache_init,     /* Init */
      default_cache_shutdown, /* Shutdown */
      default_cache_fetch,    /* Fetch */
  };

  udb_config(UDB_CONFIG_CACHE_METHOD, &default_cache_methods);
}