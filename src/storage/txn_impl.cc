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

Status TxnImpl::Write(BTree *tree, const Slice &key, const Slice &value) {
  Status status;
  CursorLocation location;

  status = cursor_->MoveTo(tree, key);
  if (!status.Ok()) {
    return status;
  }
  Assert(cursor_->IsValid());
  location = cursor_->Location();

  // If the cursor is currently pointing to the the entry, check whether
  // the size of the entry is the same as the new content, if so then use the
  // overwrite optimization.
  if (location == Equal) {
    cursor_->GetCell();
    if (cursor_->PayloadSize() == value.Size()) {
      return cursor_->Overwrite(key, value);
    }
  }
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