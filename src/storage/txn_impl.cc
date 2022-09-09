#include "storage/txn_impl.h"
#include "storage/cursor.h"

namespace udb {
TxnImpl::TxnImpl(bool write, int lockIndex)
    : write_(write), lockIndex_(lockIndex), cursor_(new Cursor(this)) {}

TxnImpl::~TxnImpl() { delete cursor_; }

Status TxnImpl::OpenTree(const std::string &name, BTree **,
                         bool createIfNotExists) {
  Status status;
  return status;
};

Status TxnImpl::DeleteTree(const std::string &name) {
  Status status;
  return status;
}

Status TxnImpl::Write(BTree *, const Slice &key, const Slice &value) {
  Status status;
  return status;
}

Status TxnImpl::Delete(BTree *, const Slice &key) {
  Status status;
  return status;
}

Status TxnImpl::Get(BTree *, const Slice &key, Slice *value) {
  Status status;
  return status;
}
} // namespace udb