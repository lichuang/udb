#include <assert.h>
#include <stdio.h>

#include "memory/memory.h"
#include "os/os.h"
#include "wal.h"

extern udb_code_t wal_open_impl_v1(wal_config_t *, wal_t **);

udb_code_t wal_open(wal_config_t *config, wal_t **wal) {
  assert(config->version == 1);

  return wal_open_impl_v1(config, wal);
}

udb_code_t wal_close(wal_t *wal) {
  wal->methods.Destroy(wal->impl);
  memory_free(wal);
}

/*
** Begin a read transaction on the database.
**
** This routine used to be called sqlite3OpenSnapshot() and with good reason:
** it takes a snapshot of the state of the WAL and wal-index for the current
** instant in time.  The current thread will continue to use this snapshot.
** Other threads might append new content to the WAL and wal-index but
** that extra content is ignored by the current thread.
**
** If the database contents have changes since the previous read
** transaction, then *pChanged is set to 1 before returning.  The
** Pager layer will use this to know that its cache is stale and
** needs to be flushed.
*/
udb_code_t walBeginReadTransaction(wal_t *wal, bool *changed) {
  return wal->methods.BeginReadTransaction(wal->impl, changed);
}

/*
** Search the wal file for page id. If found, set *frame to the frame that
** contains the page. Otherwise, if id is not in the wal file, set *frame
** to zero.
**
** Return UDB_OK if successful, or an error code if an error occurs. If an
** error does occur, the final value of *frame is undefined.
*/
udb_code_t wal_find_frame(wal_t *wal, page_no_t no, wal_frame_t *frame) {
  wal->methods.FindFrame(wal->impl, no, frame);
}

/*
** Read the contents of frame readFrame from the wal file into buffer buffer
** (which is bufferSize bytes in size). Return UDB_OK if successful, or an
** error code otherwise.
*/
udb_code_t wal_read_frame(wal_t *wal, wal_frame_t readFrame,
                          uint32_t bufferSize, void *buffer) {
  return wal->methods.ReadFrame(wal->impl, readFrame, bufferSize, buffer);
}