#ifndef _UDB_CODEC_H_
#define _UDB_CODEC_H_

#include "types.h"

uint32_t get4Byte(const uint8_t *);
void put4Byte(unsigned char *, uint32_t);

#endif /* _UDB_CODEC_H_ */