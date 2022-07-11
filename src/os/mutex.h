#ifndef _UDB_MUTEX_H_
#define _UDB_MUTEX_H_

#include "types.h"

bool mutex_held(mutex_t *);
bool mutex_not_held(mutex_t *);

void mutex_enter(mutex_t *);
void mutex_leave(mutex_t *);

#endif /* _UDB_MUTEX_H_ */