#ifndef _UDB_WAL_H_
#define _UDB_WAL_H_

#include "types.h"
#include <udb.h>

struct wal_config_t {
  int version;         /* Wal version */
  os_t *os;            /* os module to open wal and wal-index */
  file_t *dbFile;      /* The open database file */
  const char *walName; /* Name of the WAL file */
  uint64_t maxWalSize; /* Truncate WAL to this size on reset */
};

struct wal_methods_t {
  udb_code_t (*FindFrame)(wal_impl_t *, page_no_t, wal_frame_t *);

  udb_code_t (*ReadFrame)(wal_impl_t *, wal_frame_t, uint32_t, void *);

  udb_code_t (*BeginReadTransaction)(wal_impl_t *, bool *);
  void (*Destroy)(wal_impl_t *);
};

struct wal_t {
  int version;           /* Version number */
  wal_impl_t *impl;      /* Wal impl argument */
  wal_methods_t methods; /* Wal methods */
};

#define IS_VALID_WAL_FRAME(frame) (frame > 0)

/* Open and close a connection to a write-ahead log. */
udb_code_t walOpen(wal_config_t *, wal_t **);
udb_code_t walClose(wal_t *);

/* Used by readers to open (lock) and close (unlock) a snapshot.  A
** snapshot is like a read-transaction.  It is the state of the database
** at an instant in time.  sqlite3WalOpenSnapshot gets a read lock and
** preserves the current state even if the other threads or processes
** write to or checkpoint the WAL.  sqlite3WalCloseSnapshot() closes the
** transaction and releases the lock.
*/
udb_code_t walBeginReadTransaction(wal_t *, bool *);
udb_code_t walEndReadTransaction(wal_t *);

/* Read a page from the write-ahead log, if it is present. */
udb_code_t walFindFrame(wal_t *, page_no_t, wal_frame_t *);
udb_code_t walReadFrame(wal_t *, wal_frame_t, uint32_t, void *);

/* Obtain or release the WRITER lock. */
int walBeginWriteTransaction(wal_t *);
int walEndWriteTransaction(wal_t *);

#endif /* _UDB_WAL_H_ */