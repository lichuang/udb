#ifndef _UDB_OS_H_
#define _UDB_OS_H_

#include "types.h"

/*
** An instance of the os_t object defines the interface between
** the SQLite core and the underlying operating system.
*/
struct os_t {
  int version;    /* Structure version number (currently 1) */
  int sizeOfFile; /* Size of subclassed file_t */
};

udb_err_t os_open(os_t *, const char *path, file_t *file, int flags);

#endif /* _UDB_OS_H_ */