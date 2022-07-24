#include <stdio.h>

#include "global.h"
#include "misc/error.h"
#include "udb.h"

/*
** The following singleton contains the global configuration for
** the udb library.
*/
global_config_t udbGlobalConfig = {
    false, /* inited */
    NULL,  /* cacheMethods */
    NULL,  /* Log */
    NULL,  /* logArg */
};

/*
** This API allows applications to modify the global configuration of
** the SQLite library at run-time.
**
** This routine should only be called when there are no outstanding
** database connections or memory allocations.  This routine is not
** threadsafe.  Failure to heed these warnings can lead to unpredictable
** behavior.
*/
udb_code_t udb_config(int op, ...) {
  va_list ap;
  udb_code_t rc = UDB_OK;

  /* udb_config() shall return UDB_MISUSE_BKPT if it is invoked while
  ** the udb library is in use. */
  if (udbGlobalConfig.inited)
    return UDB_MISUSE_BKPT;

  va_start(ap, op);
  switch (op) {
  case UDB_CONFIG_CACHE_METHOD:
    udbGlobalConfig.cacheMethods = va_arg(ap, cache_methods_t *);
    break;
  case UDB_CONFIG_LOG:
    typedef void (*LOGFUNC_t)(void *, int, const char *);
    udbGlobalConfig.Log = va_arg(ap, LOGFUNC_t);
    udbGlobalConfig.logArg = va_arg(ap, void *);
    break;

  case UDB_CONFIG_PAGECACHE: {
    udbGlobalConfig.page = va_arg(ap, void *);
    udbGlobalConfig.pageSize = va_arg(ap, int);
    udbGlobalConfig.pageNum = va_arg(ap, int);
    break;
  }
  }
  va_end(ap);

  return rc;
}