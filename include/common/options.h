#pragma once

#include "common/export.h"
#include "common/types.h"

namespace udb {
struct UDB_EXPORT Options {
  // Create an Options object with default values for all fields.
  Options();

  // page size, MUST be a power of 2 and between [1024, 65536]
  int pageSize_ = 4096;

  // cache size
  int cacheSize_ = 1024000;
};

struct UDB_EXPORT WriteOptions {};

struct UDB_EXPORT ReadOptions {};
} // namespace udb