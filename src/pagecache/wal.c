#include <assert.h>
#include <stdio.h>

#include "memory/alloc.h"
#include "os/os.h"
#include "wal.h"

extern udb_err_t wal_open_version1(wal_config_t *, wal_t **);

udb_err_t wal_open(wal_config_t *config, wal_t **wal) {
  assert(config->version == 1);

  return wal_open_version1(config, wal);
}

udb_err_t wal_close(wal_t *wal) {
  wal->methods.Destroy(wal->impl);
  udb_free(wal);
}

udb_err_t wal_find_frame(wal_t *wal, page_id_t id, wal_frame_t *frame) {
  wal->methods.FindFrame(wal->impl, id, frame);
}