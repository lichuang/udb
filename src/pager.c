#include <assert.h>
#include <stdio.h>

#include "alloc.h"
#include "file.h"
#include "page.h"
#include "page_cache.h"
#include "pager.h"
#include "wal.h"

/*
 ** Index types of pager.stat[] array.
 */
enum pager_stat_t {
  PAGER_STAT_HIT = 0,
  PAGER_STAT_MISS = 1,
  PAGER_STAT_WRITE = 2,
  PAGER_STAT_SPILL = 3,

  PAGER_STAT_MAX
};

struct pager_t {
  udb_t *udb;

  cache_t *cache;

  wal_t *wal;

  file_t *db_file; /* the db file fd */

  unsigned int page_size;

  uint32_t stat[PAGER_STAT_MAX]; /* page cache statistics array */
};

/* Static internal function forward declarations */
static inline offset_t from_page_id_to_offset(pager_t *, page_id_t);
static udb_err_t read_db_page(page_t *);

/* Outer function implementations */

udb_err_t pager_open(udb_t *udb, pager_t **pager) {
  const char *db_path = udb->config.db_path;
  pager_t *ret_pager;
  file_t *db_file = NULL;
  wal_t *wal = NULL;
  cache_t *cache = NULL;
  udb_err_t err = UDB_OK;

  err = udb_file_open(db_path, 0, &db_file);
  if (err != UDB_OK) {
    goto open_pager_error;
  }
  err = wal_open(db_file, &wal);
  if (err != UDB_OK) {
    goto open_pager_error;
  }

  *pager = ret_pager = udb_calloc(sizeof(pager_t));
  err = cache_open(&cache);
  if (err != UDB_OK) {
    goto open_pager_error;
  }

  ret_pager->db_file = db_file;
  ret_pager->wal = wal;
  ret_pager->cache = cache;
  ret_pager->udb = udb;
  ret_pager->page_size = udb->config.page_size;

  return UDB_OK;

open_pager_error:
  if (db_file != NULL) {
    udb_file_close(db_file);
  }
  if (wal != NULL) {
    wal_close(wal);
  }
  if (ret_pager != NULL) {
    udb_free(ret_pager);
  }
  *pager = NULL;
  return err;
}

udb_err_t pager_close(pager_t *pager) {
  udb_free(pager);
  return UDB_OK;
}

udb_err_t pager_get_page(pager_t *pager, page_id_t id, page_t **page) {
  page_t *pg = NULL;
  page_t *item = NULL;
  cache_t *cache = pager->cache;
  udb_err_t err = UDB_OK;

  assert(id > 0);

  *page = NULL;
  item = cache_fetch(cache, id);
  if (item == NULL) {
    err = cache_fetch_stress(cache, id, &item);
    if (err != UDB_OK) {
      goto get_page_error;
    }
    if (item == NULL) {
      err = UDB_OOM;
    }
    return err;
  }
  pg = *page = cache_fetch_finish(pager->cache, id, item);

  assert(pg->id == id);
  assert(pg->pager == pager || pg->pager == NULL);

  /*
  ** If page->pager is not NULL, means that the cache has an initialized copy
  ** of page.
  */
  if (pg->pager != NULL) {
    pager->stat[PAGER_STAT_HIT]++;
    return UDB_OK;
  }

  /*
  ** In this case the pager cache has created a new page.
  ** Its content need to be initialized(read content from disk, etc.).
  */
  pg->pager = pager;
  pager->stat[PAGER_STAT_MISS]++;
  err = read_db_page(pg);
  if (err != UDB_OK) {
    goto get_page_error;
  }
  return UDB_OK;

get_page_error:
  if (item != NULL) {
    cache_drop(cache, item);
  }
  *page = NULL;
  return err;
}

/* Static internal function implementations */

/*
 ** Convert the page id to database file offset.
 */
static inline offset_t from_page_id_to_offset(pager_t *pager, page_id_t id) {
  return (id - 1) * pager->page_size;
}

/*
 ** Read the content of page from the database file.
 */
static udb_err_t read_db_page(page_t *page) {
  pager_t *pager = page->pager;
  page_id_t id = page->id;
  udb_err_t err = UDB_OK;
  wal_frame_t frame;
  wal_t *wal = pager->wal;
  file_t *db = pager->db_file;
  offset_t offset;

  err = wal_find_frame(wal, id, &frame);
  if (err != UDB_OK) {
    return err;
  }

  if (IS_VALID_WAL_FRAME(frame)) {
    /* In this case the current page in the wal frame, just read it from wal */
    err = wal_read_frame(wal, frame, pager->page_size, page->data);
  } else {
    /* Read the page from database file */
    offset = from_page_id_to_offset(pager, id);
    err = udb_file_read(db, page->data, pager->page_size, offset);
  }

  if (err != UDB_OK) {
    return err;
  }

  return err;
}