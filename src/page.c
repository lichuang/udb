#include <assert.h>

#include "page.h"

udb_code_t page_set_writable(page_t *page) { return UDB_OK; }

void page_mark_dirty(page_t *page) {
  assert(page->ref > 0);

  if (page->flags & (PAGE_FLAG_CLEAN | PAGE_FLAG_DONT_WRITE)) {
    page->flags &= ~PAGE_FLAG_DONT_WRITE;
    if (page->flags & PAGE_FLAG_CLEAN) {
    }
  }
}