#ifndef _UDB_PAGER_H_
#define _UDB_PAGER_H_

#include "types.h"
#include <udb.h>

udb_code_t pager_open(udb_t *, pager_t **);
udb_code_t pager_close(pager_t *);

udb_code_t pager_get_page(pager_t *, page_no_t, page_t **);

#endif /* _UDB_PAGER_H_ */