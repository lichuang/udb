#ifndef _UDB_ALLOC_H_
#define _UDB_ALLOC_H_

#include "types.h"

void *udb_calloc(uint32_t size);
void udb_free(void *);

#endif /* _UDB_ALLOC_H_ */