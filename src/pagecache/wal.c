#include <stdio.h>

#include "memory/alloc.h"
#include "os/os.h"
#include "wal.h"

/*
** An open write-ahead log file is represented by an instance of the
** following object.
*/
struct wal_t {
  os_t *os;        /* The os object used to create dbFile */
  file_t *dbFile;  /* File handle for the database file */
  file_t *walFile; /* File handle for WAL file */
};

/* Static internal function forward declarations */

/* Static internal function implementations */

/* Outer function implementations */
udb_err_t wal_open(wal_config_t *config, wal_t **wal) {
  udb_err_t ret = UDB_OK;
  wal_t *retWal = NULL;
  os_t *os = config->os; /* os module to open wal and wal-index */
  file_t *dbFile;        /* The open database file */
  const char *walName = config->walName; /* Name of the WAL file */

  assert(walName != NULL && walName[0] != '\0');
  assert(config->dbFile != NULL);

  *wal = NULL;

  retWal = (wal_t *)udb_calloc(sizeof(wal_t) + os->sizeOfFile);
  if (retWal == NULL) {
    return UDB_OOM;
  }

  retWal->os = os;
  retWal->walFile = (file_t *)&retWal[1];
  retWal->dbFile = config->dbFile;

  return ret;
}