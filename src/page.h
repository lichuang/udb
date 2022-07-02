#ifndef _UDB_PAGE_H_
#define _UDB_PAGE_H_

#include <udb.h>

#include <stdio.h>

#include "types.h"

/* Page flag bits */
#define PAGE_FLAG_CLEAN 0x001      /* Page not on the PCache.pDirty list */
#define PAGE_FLAG_DIRTY 0x002      /* Page is on the PCache.pDirty list */
#define PAGE_FLAG_DONT_WRITE 0x010 /* Do not write content to disk */

/* page_t represents the page read from the database file */
struct page_t {
  page_id_t id;

  pager_t *pager;

  page_cache_t *cache; /* Cache that owns this page */
  uint16_t flags;

  uint16_t ref; /* Number of reference count of this page */

  void *data; /* The page data read from file */

  page_t *dirty_next; /* Next element in list of dirty pages */
  page_t *dirty_prev; /* Prev element in list of dirty pages */
};

udb_err_t page_set_writable(page_t *);

void page_mark_dirty(page_t *);

#endif /* _UDB_PAGE_H_ */
