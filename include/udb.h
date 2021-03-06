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

typedef enum udb_code_t {
  UDB_OK = 0,         /* Success */
  UDB_OOM = -1,       /* out of memory */
  UDB_MISUSE = -2,    /* Library used incorrectly */
  UDB_BUSY = -3,      /* The database file is locked */
  UDB_CORRUPT = -4,   /* The database disk image is malformed */
  UDB_WAL_RETRY = -5, /* read wal returns when it needs to be retried. */
  UDB_PROTOCOL = -6,  /* Database lock protocol error */
} udb_code_t;

#endif /* _UDB_H_ */