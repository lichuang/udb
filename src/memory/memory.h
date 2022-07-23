#ifndef _UDB_ALLOC_H_
#define _UDB_ALLOC_H_

#include "types.h"

void *memory_alloc(uint32_t size);
void *memory_calloc(uint32_t size);
void *memory_realloc(void *p, uint32_t size);
void memory_free(void *);

#endif /* _UDB_ALLOC_H_ */