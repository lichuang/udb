#pragma once

#include "common/export.h"
#include "common/options.h"
#include "common/slice.h"
#include "common/status.h"
#include "common/types.h"

namespace udb {
class UDB_EXPORT BPTree {
public:
  BPTree(const Options &options, const string &name);

  BPTree(const BPTree &) = delete;
  BPTree &operator=(const BPTree &) = delete;

  ~BPTree();

  // begin and commit transaction.
  Status BeginTxn(bool isWrite);
  Status Commit();

  Status Write(const WriteOptions &options, const Slice &key,
               const Slice &value);
  Status Delete(const WriteOptions &options, const Slice &key);
  Status Get(const ReadOptions &options, const Slice &key, std::string *value);

private:
  Status MoveTo(const Slice &key, BptCursor *);

private:
  Env *env_;
  File *db_file_;
  BufferManager *buffer_manager_;
};
} // namespace udb