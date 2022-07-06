#include <assert.h>
#include <stdio.h>

#include "global.h"
#include "memory/alloc.h"
#include "page.h"
#include "pagecache/page_cache.h"

struct page_cache_t {
  page_t *dirty, *dirty_tail;
};

/* Static internal function forward declarations */

/* Outer function implementations */

udb_err_t cache_open(page_cache_t **cache) {
  udb_err_t err = UDB_OK;
  page_cache_t *ret_cache = NULL;

  *cache = ret_cache = (page_cache_t *)udb_calloc(sizeof(page_cache_t));
  if (ret_cache == NULL) {
    return UDB_OOM;
  }

  return err;
}

void cache_close(page_cache_t *cache) { udb_free(cache); }

/*
 ** fetch an item from cache by the page id.
 */
page_t *cache_fetch(page_cache_t *cache, page_id_t id,
                    cache_create_flag_t create_flag) {
  cache_item_t *item = udbGlobalConfig.cacheMethods->Fetch(
      udbGlobalConfig.cacheMethods->arg, id, create_flag);

  return NULL;
}
