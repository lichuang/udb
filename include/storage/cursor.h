#pragma once

#include "common/limits.h"
#include "common/slice.h"
#include "common/status.h"
#include "common/types.h"
#include "storage/cell.h"

namespace udb {
class BTree;
class MemPage;
class TxnImpl;

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

  bool IsReseted() const;

  void Reset();
  Status MoveTo(BTree *, const Slice &key);

  CursorLocation Location() const { return location_; }
  uint16_t KeySize() const { return cell_.KeySize(); }
  uint16_t PayloadSize() const { return cell_.PayloadSize(); }

  bool IsValid() const { return location_ != Invalid; }

  Cell *MutCell() { return &cell_; }

  int8_t CellIndex() const { return cellIndex_; }
  void GetCell();

  Status Overwrite(const Slice &key, const Slice &value);

private:
  Status MoveToRoot();
  Status MoveToChild(PageNo chidNo);

  void ParseCell();

private:
  TxnImpl *txn_;
  Slice key_;
  BTree *tree_;
  Cell cell_;   // A parse of the cell we are pointing at.
  PageNo root_; // root page no of BTree
  CursorLocation location_;
  int8_t cellIndex_;                      // Index of cursor in current page.
  int8_t curIndex_;                       // Index of current page in pageStack_
  MemPage *page_;                         // current page
  MemPage *pageStack_[kTreeMaxDepth - 1]; // Stack of parents of current page
};
} // namespace udb