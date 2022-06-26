#ifndef _UDB_WAL_H_
#define _UDB_WAL_H_

#include "types.h"
#include <udb.h>

#define IS_VALID_WAL_FRAME(frame) (frame > 0)

udb_err_t wal_open(file_t *db_file, wal_t **);
udb_err_t wal_close(wal_t *);

udb_err_t wal_find_frame(wal_t *, page_id_t, wal_frame_t *);
udb_err_t wal_read_frame(wal_t *, wal_frame_t, uint32_t, void *);

#endif /* _UDB_WAL_H_ */