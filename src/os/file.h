#ifndef _UDB_FILE_H_
#define _UDB_FILE_H_

#include "types.h"
#include <udb.h>

struct file_t {
  char *path;
};

enum udb_open_file_flags_t {
  UDB_OPEN_READONLY = 0x1,
  UDB_OPEN_READWRITE = 0x2,
  UDB_OPEN_CREATE = 0x4,
};

#define udb_file_is_open(file)

udb_err_t udb_file_close(file_t *);

udb_err_t udb_file_read(file_t *, void *, uint32_t, offset_t);

#endif /* _UDB_FILE_H_ */