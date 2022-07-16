#ifndef _UDB_H_
#define _UDB_H_

typedef struct udb_config_t {
  const char *db_path;
  int pageSize;                /* page size */
  int preAllocatePageCacheNum; /* pre-allocate number of page cache */
} udb_config_t;

typedef struct udb_t {
  udb_config_t config;
} udb_t;

typedef enum udb_err_t {
  UDB_OK = 0,      /* Success */
  UDB_OOM = -1,    /* out of memory */
  UDB_MISUSE = -2, /* Library used incorrectly */
  UDB_BUSY = -3,   /* The database file is locked */
} udb_err_t;

#endif /* _UDB_H_ */