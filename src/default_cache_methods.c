#include <assert.h>
#include <stdio.h>

#include "alloc.h"
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
  bool is_bulk_local;     /* This item from bulk local storage */
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
  default_cache_group_t *group; /* Group this cache belongs to */
  int page_size;                /* Size of database page */
  int extra_size;               /* sizeof(node_t)+sizeof(page_t) */
  int cache_line_size;          /* Total size of one cache line */
  bool purgeable;               /* True if cache is purgeable */
  uint32_t min_item_num;        /* The Minimum number of items reserved */
  uint32_t max_item_num;        /* Configured "cache_size" value */
  uint32_t n_90_percent;        /* max_item_num*9/10 */
  uint32_t item_num;            /* Total number of items in hash */
  uint32_t recyclable;          /* Number of pages in the LRU list */
  uint32_t slot_num;            /* Number of slots in hash[] */
  default_cache_item_t **hash;  /* Hash table for lookup by key */
  default_cache_item_t *free;   /* List of unused pcache-local pages */
};

/*
** Global data used by default cache.
*/
static struct default_cache_global_t {
  int init_item_num; /* Initial bulk allocation size */
} default_cache_global;

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
static bool init_bulk(default_cache_t *);

static default_cache_item_t *pin_item(default_cache_item_t *);

static default_cache_item_t *fetch_no_mutex(default_cache_t *, page_id_t key,
                                            cache_create_flag_t flag);

static default_cache_item_t *fetch_stage2(default_cache_t *cache, page_id_t key,
                                          cache_create_flag_t flags);

static void cache_resize_hash(default_cache_t *cache);

/* Static function implementations */

static cache_item_t *default_cache_fetch(cache_methods_t *method, page_id_t key,
                                         cache_create_flag_t flag) {
  return (cache_item_t *)fetch_no_mutex((default_cache_t *)method->arg, key,
                                        flag);
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
          (is_cache_under_memory_pressure(cache) &&
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
      is_cache_under_memory_pressure(cache)) {
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
  h = item->key % cache->slot_num;
  for (prev_item = &cache->hash[h]; (*prev_item) != item;
       prev_item = &(*prev_item)->next)
    ;
  *prev_item = (*prev_item)->next;

  cache->item_num -= 1;
  if (free_flag) {
    free_item(item);
  }
}

static void free_item(default_cache_item_t *item) {}

/*
** Allocate a new item object initially associated with cache.
*/
static default_cache_item_t *alloc_item(default_cache_t *cache) {
  default_cache_item_t *item;
  void *data;

  assert(mutex_held(cache->group->mutex));

  if (cache->free || (cache->item_num == 0 && init_bulk(cache))) {
    assert(cache->free != NULL);
    /* free the item from free list */
    item = cache->free;
    cache->free = item->next;
    item->next = NULL;
  } else {
    data = udb_alloc(cache->cache_line_size);
    if (data == NULL) {
      return NULL;
    }
    item = (default_cache_item_t *)&((uint8_t *)data)[cache->page_size];
    item->base.buf = data;
    item->base.extra = &item[1];
    item->is_bulk_local = false;
    item->is_anchor = false;
    item->lru_prev = NULL;
  }
  cache->group->purgeable_item += 1;

  return item;
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

/*
** Try to initialize the cache->free and pCache->pBulk fields.
** Return true if cache->free ends up containing one or more free pages.
*/
static bool init_bulk(default_cache_t *cache) {
  int64_t bulk_size;
  int bulk_num;
  char *bulk;
  default_cache_item_t *item;

  if (default_cache_global.init_item_num == 0) {
    return false;
  }

  /* Do not bother with a bulk allocation if the cache size very small */
  if (cache->max_item_num < 3) {
    return false;
  }

  if (default_cache_global.init_item_num > 0) {
    bulk_size = cache->cache_line_size * default_cache_global.init_item_num;
  } else {
    bulk_size = -1024 * default_cache_global.init_item_num;
  }
  if (bulk_size > cache->cache_line_size * cache->max_item_num) {
    bulk_size = cache->cache_line_size * cache->max_item_num;
  }

  bulk = udb_alloc(bulk_size);
  if (bulk == NULL) {
    return false;
  }

  /* init the bulk cache items */
  bulk_num = bulk_size / cache->cache_line_size;
  do {
    item = (default_cache_item_t *)&bulk[cache->page_size];
    item->base.extra = &item[1];
    item->is_anchor = false;
    item->is_bulk_local = true;
    item->next = cache->free;
    item->lru_prev = NULL;
    cache->free = item;
    bulk += cache->cache_line_size;
  } while (--bulk_num);

  return cache->free != NULL;
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

struct cache_methods_t *default_cache_methods;
/*
struct cache_methods_t default_cache_methods = {

};
*/