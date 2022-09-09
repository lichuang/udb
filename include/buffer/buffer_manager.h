#pragma once

#include "common/export.h"
#include "common/status.h"
#include "common/types.h"
#include "udb.h"

namespace udb {

class MemPage;

class BufferManager {
public:
  BufferManager(const Options &options, const string &path);
  ~BufferManager();

  static BufferManager *Instance();

  Status GetPage(PageNo no, MemPage **page);

private:
  int pageSize_;
  int cacheSize_;
  string dbName_;
};

#define Pager BufferManager::Instance()

} // namespace udb