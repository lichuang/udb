#pragma once

#include "common/export.h"
#include "common/options.h"
#include "common/types.h"

namespace udb {
class BufferManager {
public:
  BufferManager(const Options &options, const string &name);
  ~BufferManager();

private:
  int pageSize_;
  int cacheSize_;
  string dbName_;
};
} // namespace udb