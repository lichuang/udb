#include "buffer/buffer_manager.h"

namespace udb {
BufferManager::BufferManager(const Options &options, const string &name)
    : pageSize_(options.pageSize_), cacheSize_(options.cacheSize_),
      dbName_(name) {}

BufferManager::~BufferManager() {}
} // namespace udb