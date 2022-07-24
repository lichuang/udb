#ifndef _UDB_GLOBAL_CONFIG_H_
#define _UDB_GLOBAL_CONFIG_H_

#include <stdarg.h> /* Needed for the definition of va_list */

#include "types.h"

/*
** These constants are the available integer configuration options that
** can be passed as the first argument to the [udb_config()] interface.
*/
#define UDB_CONFIG_CACHE_METHOD 1 /* cache_methods_t */
#define UDB_CONFIG_LOG 2          /* Log func and arg */
#define UDB_CONFIG_PAGECACHE 3    /* void*, int sz, int N */

/*
** Structure containing global configuration data for the udb library.
**
** This structure also contains some state information.
*/
struct global_config_t {
  bool inited;                   /* True after initialization has finished */
  cache_methods_t *cacheMethods; /* Low-level page-cache interface */
  void (*Log)(void *, int, const char *); /* Function for logging */
  void *logArg;                           /* First argument to Log() */
  void *page;                             /* Page cache memory */
  int pageSize;                           /* Size of each page in page[] */
  int pageNum;                            /* Number of pages in page[] */
};

udb_code_t udb_config(int, ...);

extern global_config_t udbGlobalConfig;

#endif /* _UDB_GLOBAL_CONFIG_H_ */