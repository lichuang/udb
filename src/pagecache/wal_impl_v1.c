#include <stdio.h>

#include "memory/alloc.h"
#include "os/file.h"
#include "os/os.h"
#include "wal.h"

/*
** An open write-ahead log file is represented by an instance of the
** following object.
*/
typedef struct wal_impl_v1_t {
  os_t *os;            /* The os object used to create dbFile */
  file_t *dbFile;      /* File handle for the database file */
  file_t *walFile;     /* File handle for WAL file */
  uint64_t maxWalSize; /* Truncate WAL to this size upon reset */
} wal_impl_v1_t;

/* Static wal impl methods function forward declarations */
udb_err_t __FindFrame_v1(wal_impl_t *, page_id_t, wal_frame_t *);
void __Destroy_v1(wal_impl_t *);

/* Static internal function forward declarations */
static void __init_wal_impl_v1(wal_t *, wal_impl_v1_t *);

/* Static wal impl methods function forward declarations */
udb_err_t __FindFrame_v1(wal_impl_t *impl, page_id_t id, wal_frame_t *frame) {}

void __Destroy_v1(wal_impl_t *impl) {}

/* Static internal function implementations */

/* Outer function implementations */

/*
** Open a connection to the WAL file config->walName. The database file must
** already be opened on connection config->dbFile.
** The buffer that config->walName points
** to must remain valid for the lifetime of the returned wal_t* handle.
**
** A SHARED lock should be held on the database file when this function
** is called. The purpose of this SHARED lock is to prevent any other
** client from unlinking the WAL or wal-index file. If another process
** were to do this just after this client opened one of these files, the
** system would be badly broken.
**
** If the log file is successfully opened, UDB_OK is returned and
** *wal is set to point to a new WAL handle. If an error occurs,
** an UDB error code is returned and *wal is left unmodified.
*/
udb_err_t wal_open_impl_v1(wal_config_t *config, wal_t **wal) {
  udb_err_t ret = UDB_OK;
  wal_t *retWal = NULL;
  int flags;             /* Flags passed to OsOpen() */
  os_t *os = config->os; /* os module to open wal and wal-index */
  file_t *dbFile;        /* The open database file */
  const char *walName = config->walName; /* Name of the WAL file */
  wal_impl_v1_t *impl = NULL;

  assert(config->version == 1);
  assert(walName != NULL && walName[0] != '\0');
  assert(config->dbFile != NULL);

  *wal = NULL;

  retWal = (wal_t *)udb_calloc(sizeof(wal_t) + sizeof(wal_impl_v1_t) +
                               os->sizeOfFile);
  if (retWal == NULL) {
    return UDB_OOM;
  }

  impl = (wal_impl_v1_t *)&retWal[1];
  impl->walFile = (file_t *)&impl[1];
  __init_wal_impl_v1(retWal, impl);

  impl->os = os;
  impl->walFile = (file_t *)&impl[1];
  impl->dbFile = config->dbFile;
  impl->maxWalSize = config->maxWalSize;

  /* Open file handle on the write-ahead log file. */
  flags = (UDB_OPEN_READWRITE | UDB_OPEN_CREATE);
  ret = os_open(os, walName, impl->walFile, flags);
  if (ret != UDB_OK) {
    udb_free(retWal);
  }

  return ret;
}

/* Static internal function implementations */
static void __init_wal_impl_v1(wal_t *wal, wal_impl_v1_t *impl) {
  wal->impl = impl;
  wal->version = 1;

  wal->methods = (wal_methods_t){
      __FindFrame_v1, /* FindFrame */
      __Destroy_v1,   /* Destroy */
  };
}