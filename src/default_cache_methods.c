#include <assert.h>
#include <stdio.h>

#include "cache.h"
#include "mutex.h"
#include "page_cache.h"

typedef struct default_cache_t default_cache_t;
typedef struct default_cache_group_t default_cache_group_t;
typedef struct default_cache_item_t default_cache_item_t;

struct default_cache_item_t {
  cache_item_t base;      /* Base class, Must be first. */
  page_id_t key;          /* Key value (page id) */
  default_cache_t *cache; /* Cache that currently owns this item */
  bool is_anchor;         /* This is the default_cache_group_t.lru element */
  default_cache_item_t *next;     /* Next item in hash table chain */
  default_cache_item_t *lru_next; /* Next page in LRU list of unpined items */
  default_cache_item_t *lru_prev; /* Prev page in LRU list of unpined items */
};

struct default_cache_group_t {
  mutex_t *mutex;
  uint32_t max_item_num;    /* Sum of nMax for purgeable caches */
  uint32_t min_item_num;    /* Sum of nMin for purgeable caches */
  uint32_t max_pinned_item; /* nMaxpage + 10 - nMinPage */
  uint32_t purgeable_item;  /* Number of purgeable pages allocated */
  default_cache_item_t lru; /* The beginning and end of the LRU list */
};

struct default_cache_t {
  default_cache_group_t *group;    /* Group this cache belongs to */
  int cache_line_size;             /* Total size of one cache line */
  bool purgeable;                  /* True if cache is purgeable */
  uint32_t min_item_num;           /* The Minimum number of items reserved */
  uint32_t max_item_num;           /* Configured "cache_size" value */
  uint32_t n_90_percent;           /* max_item_num*9/10 */
  uint32_t item_num;               /* Total number of items in hash */
  uint32_t recyclable;             /* Number of pages in the LRU list */
  uint32_t slot_num;               /* Number of slots in hash[] */
  default_cache_item_t **hash;     /* Hash table for lookup by key */
  default_cache_item_t *free_list; /* List of unused pcache-local pages */
};

/* Static internal function forward declarations */

/* The default cache method implementations */
static cache_item_t *default_cache_fetch(cache_methods_t *method, page_id_t key,
                                         cache_create_flag_t flag);

/* The static internal functions */

static bool is_cache_under_memory_pressure(default_cache_t *cache);
static bool is_cache_nearly_full(default_cache_t *cache);

static default_cache_item_t *try_recycle_item(default_cache_t *cache);
static void remove_from_hash(default_cache_item_t *, bool free_item);
static void free_item(default_cache_item_t *);
static default_cache_item_t *alloc_item(default_cache_t *);
static default_cache_item_t *init_item(default_cache_t *, page_id_t,
                                       default_cache_item_t *);

static default_cache_item_t *pin_item(default_cache_item_t *);

static default_cache_item_t *fetch_no_mutex(default_cache_t *, page_id_t key,
                                            cache_create_flag_t flag);

static default_cache_item_t *fetch_stage2(default_cache_t *cache, page_id_t key,
                                          cache_create_flag_t flags);

static void cache_resize_hash(default_cache_t *cache);

/* Static function implementations */

static cache_item_t *default_cache_fetch(cache_methods_t *method, page_id_t key,
                                         cache_create_flag_t flag) {
  return (cache_item_t *)fetch_no_mutex(method, key, flag);
}

static bool is_cache_under_memory_pressure(default_cache_t *cache) {
  return false;
}

static bool is_cache_nearly_full(default_cache_t *cache) {
  uint32_t pinned_num;
  default_cache_group_t *group = cache->group;

  assert(cache->item_num >= cache->recyclable);

  pinned_num = cache->item_num - cache->recyclable;
  return ((pinned_num >= group->max_pinned_item ||
           pinned_num >= cache->n_90_percent) ||
          (cache_is_under_memory_pressure(cache) &&
           cache->recyclable < pinned_num));
}

static default_cache_item_t *try_recycle_item(default_cache_t *cache) {
  default_cache_group_t *group = cache->group;
  default_cache_t *other;
  default_cache_item_t *item = NULL;

  if (group->lru.lru_prev->is_anchor) {
    return NULL;
  }

  if (cache->item_num + 1 < cache->max_item_num &&
      cache_is_under_memory_pressure(cache)) {
    return NULL;
  }

  item = group->lru.lru_prev;
  assert(ITEM_IS_UNPINNED(item));
  remove_from_hash(item, false);
  pin_item(item);
  other = item->cache;
  if (other->cache_line_size != cache->cache_line_size) {
    free_item(item);
    item = NULL;
  } else {
    group->purgeable_item -= (other->purgeable - cache->purgeable);
  }

  return item;
}

static void remove_from_hash(default_cache_item_t *item, bool free_item) {}

static void free_item(default_cache_item_t *item) {}

/*
** Allocate a new item object initially associated with cache.
*/
static default_cache_item_t *alloc_item(default_cache_t *cache) {
  assert(mutex_held(cache->group->mutex));
}

static default_cache_item_t *init_item(default_cache_t *cache, page_id_t key,
                                       default_cache_item_t *item) {
  assert(cache != NULL);
  assert(item != NULL);

  uint32_t h = key % cache->slot_num;
  cache->item_num += 1;
  item->key = key;
  item->next = cache->hash[h];
  item->cache = cache;
  item->lru_next = NULL;
  *(void **)item->base.extra = NULL;
  cache->hash[h] = item;

  return item;
}

static default_cache_item_t *fetch_no_mutex(default_cache_t *cache,
                                            page_id_t key,
                                            cache_create_flag_t flag) {
  default_cache_item_t *item = NULL;

  assert(cache != NULL);

  /* Step 1: Search the hash table for an existing entry. */
  item = cache->hash[key % cache->slot_num];
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

  if (cache->item_num >= cache->slot_num) {
    cache_resize_hash(cache);
  }
  assert(cache->slot_num > 0 && cache->hash != NULL);

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

static default_cache_item_t *pin_item(default_cache_item_t *item) {
  assert(item != NULL);
  assert(ITEM_IS_UNPINNED(item));
  assert(item->lru_next);
  assert(item->lru_prev);

  /* Remove the page from the LRU list */
  item->lru_next->lru_prev = item->lru_prev;
  item->lru_prev->lru_next = item->lru_next;

  item->lru_next = item->lru_prev = NULL;

  item->cache->recyclable -= 1;

  return item;
}

struct cache_methods_t default_cache_methods = {

};