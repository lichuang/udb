#include "storage/cursor.h"
#include "common/debug.h"
#include "storage/btree.h"

namespace udb {
Cursor::Cursor(TxnImpl *txn)
    : txn_(txn), tree_(nullptr), root_(kInvalidPageNo) {}

Cursor::~Cursor() {}

void Cursor::Reset() {
  tree_ = nullptr;
  root_ = kInvalidPageNo;
}

void MoveTo(const BTree *tree, const Slice &key) { Assert(IsReseted()); }

} // namespace udb