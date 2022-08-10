#pragma once

#include "common/export.h"
#include "common/options.h"
#include "common/slice.h"
#include "common/status.h"
#include "common/types.h"

namespace udb {
// B+Tree implementation
class UDB_EXPORT Tree {
public:
  Tree(const Options &options, const string &name, Os *os);

  Tree(const Tree &) = delete;
  Tree &operator=(const Tree &) = delete;

  ~Tree();

  PageNo RootPage() const { return root_; }

  // begin and commit transaction.
  Status BeginTxn(bool isWrite);
  Status Commit();

  Status Write(const WriteOptions &options, const Slice &key,
               const Slice &value);
  Status Delete(const WriteOptions &options, const Slice &key);
  Status Get(const ReadOptions &options, const Slice &key, std::string *value);

private:
  Status MoveTo(Cursor *);

private:
  Os *os_;
  PageNo root_; // Root page number.
  File *db_file_;
  BufferManager *buffer_manager_;
};
} // namespace udb