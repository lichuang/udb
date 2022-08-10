#include "storage/tree.h"
#include "buffer/buffer_manager.h"

namespace udb {
Tree::Tree(const Options &options, const string &name, Os *os)
    : os_(os), root_(kInvalidPageNo), db_file_(nullptr),
      buffer_manager_(nullptr) {}

Tree::~Tree() {}

Status Tree::BeginTxn(bool isWrite) {}

Status Tree::Commit() {}

Status Tree::Write(const WriteOptions &options, const Slice &key,
                   const Slice &value) {
  Status s;

  s = BeginTxn(true);
  if (!s.Ok()) {
    return s;
  }

  return Commit();
}

Status Tree::Delete(const WriteOptions &options, const Slice &key) {}

Status Tree::Get(const ReadOptions &options, const Slice &key,
                 std::string *value) {}

} // namespace udb