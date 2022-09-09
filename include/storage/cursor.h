#pragma once

#include "common/limits.h"
#include "common/slice.h"
#include "common/status.h"

namespace udb {
class TxnImpl;
class BTree;

const static int kCursorMaxDepth = 20;

enum CursorLocation {
  Invalid = 0,
  Left,
  Equal,
  Right,
};

// Cursor of b+tree
class UDB_EXPORT Cursor {
public:
  Cursor(TxnImpl *);

  Cursor(const Cursor &) = delete;
  Cursor &operator=(const Cursor &) = delete;

  ~Cursor();

  bool IsReseted() const { return tree_ == nullptr && location_ == Invalid; }

  void Reset();
  Status MoveTo(BTree *, const Slice &key);

private:
  Status MoveToRoot();
  Status MoveToChild(PageNo chidNo);

private:
  TxnImpl *txn_;
  Slice key_;
  BTree *tree_;
  PageNo root_; // root page no of BTree
  CursorLocation location_;
  int curIndex_;                      // Index of current page in pageStack_
  MemPage *page_;                     // current page
  MemPage *pageStack_[kMaxDepth - 1]; // Stack of parents of current page
};
} // namespace udb