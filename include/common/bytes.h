#pragma once

#include <stdint.h>

namespace udb {
#define get2byte(x) ((x)[0] << 8 | (x)[1])
#define put2byte(p, v) ((p)[0] = (uint8_t)((v) >> 8), (p)[1] = (uint8_t)(v))

uint32_t Get4Byte(const char *);
void Put4Byte(char *, uint32_t);

} // namespace udb