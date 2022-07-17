#ifndef _UDB_WAL_H_
#define _UDB_WAL_H_

#include "types.h"
#include <udb.h>

struct wal_config_t {
  os_t *os;            /* os module to open wal and wal-index */
  file_t *dbFile;      /* The open database file */
  const char *walName; /* Name of the WAL file */
  uint64_t maxWalSize; /* Truncate WAL to this size on reset */
};

#define IS_VALID_WAL_FRAME(frame) (frame > 0)

/* Open and close a connection to a write-ahead log. */
udb_err_t wal_open(wal_config_t *, wal_t **);
udb_err_t wal_close(wal_t *);

udb_err_t wal_find_frame(wal_t *, page_id_t, wal_frame_t *);
udb_err_t wal_read_frame(wal_t *, wal_frame_t, uint32_t, void *);

#endif /* _UDB_WAL_H_ */