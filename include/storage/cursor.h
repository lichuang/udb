#pragma once

#include "common/slice.h"

namespace udb {
class TxnImpl;
class BTree;

const static int kCursorMaxDepth = 20;

// Cursor of b+tree
class UDB_EXPORT Cursor {
public:
  Cursor(TxnImpl *);

  Cursor(const Cursor &) = delete;
  Cursor &operator=(const Cursor &) = delete;

  ~Cursor();

  bool IsReseted() const { return tree_ == nullptr; }

  void Reset();
  void MoveTo(const BTree *, const Slice &key);

private:
  TxnImpl *txn_;
  Slice key_;
  BTree *tree_;
  PageNo root_;
};
} // namespace udb