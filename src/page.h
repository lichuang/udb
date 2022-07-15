#ifndef _UDB_PAGE_H_
#define _UDB_PAGE_H_

#include <udb.h>

#include <stdio.h>

#include "types.h"

/*
** Every page in the cache is controlled by an instance of the following
** structure.
*/
struct page_t {
  cache_item_base_t *base;
  void *data;          /* Page data */
  void *extra;         /* Extra content */
  page_cache_t *cache; /* PRIVATE: Cache that owns this page */
  page_t *dirty;       /* Transient list of dirty sorted by pgno */
  pager_t *pager;      /* The pager this page is part of */
  page_id_t id;        /* Page number for this page */
  uint16_t flags;      /* PAGE_FLAG flags defined below */

  /**********************************************************************
  ** Elements above, except cache, are public.  All that follow are
  ** private to pcache.c and should not be accessed by other modules.
  ** cache is grouped with the public elements for efficiency.
  */
  uint16_t refNum;   /* Number of reference count of this page */
  page_t *dirtyNext; /* Next element in list of dirty pages */
  page_t *dirtyPrev; /* Prev element in list of dirty pages */
};

/* Page flag bits */
#define PAGE_FLAG_CLEAN 0x001      /* Page not on the page_cache_t.dirty list */
#define PAGE_FLAG_DIRTY 0x002      /* Page is on the page_cache_t.dirty list */
#define PAGE_FLAG_DONT_WRITE 0x010 /* Do not write content to disk */

udb_err_t page_set_writable(page_t *);

void page_mark_dirty(page_t *);

#endif /* _UDB_PAGE_H_ */
