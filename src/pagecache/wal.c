#include <assert.h>
#include <stdio.h>

#include "memory/memory.h"
#include "os/os.h"
#include "wal.h"

extern udb_err_t wal_open_impl_v1(wal_config_t *, wal_t **);

udb_err_t wal_open(wal_config_t *config, wal_t **wal) {
  assert(config->version == 1);

  return wal_open_impl_v1(config, wal);
}

udb_err_t wal_close(wal_t *wal) {
  wal->methods.Destroy(wal->impl);
  memory_free(wal);
}

/*
** Search the wal file for page id. If found, set *frame to the frame that
** contains the page. Otherwise, if id is not in the wal file, set *frame
** to zero.
**
** Return UDB_OK if successful, or an error code if an error occurs. If an
** error does occur, the final value of *frame is undefined.
*/
udb_err_t wal_find_frame(wal_t *wal, page_no_t no, wal_frame_t *frame) {
  wal->methods.FindFrame(wal->impl, id, frame);
}