#pragma once

#include "common/limits.h"
#include "common/slice.h"
#include "common/status.h"
#include "common/types.h"
#include "storage/cell.h"
#include "storage/storage_types.h"

namespace udb {
class BTree;
class MemPage;
class TxnImpl;

// Cursor of b+tree
class UDB_EXPORT Cursor {
public:
  Cursor(TxnImpl *);

  Cursor(const Cursor &) = delete;
  Cursor &operator=(const Cursor &) = delete;

  ~Cursor();

  bool IsReseted() const;

  void Reset();
  Code MoveTo(BTree *, const Slice &key);

  CursorLocation Location() const { return location_; }
  uint16_t KeySize() const { return cell_.KeySize(); }
  uint16_t PayloadSize() const { return cell_.PayloadSize(); }

  bool IsValid() const { return location_ != Invalid; }

  Cell *MutCell() { return &cell_; }

  int8_t CellIndex() const { return cellIndex_; }
  void GetCell();

  Status Overwrite(const Slice &key, const Slice &value);

private:
  Code MoveToRoot();
  Code MoveToChild(PageNo chidNo);

  void ParseCell();
  void Reset();

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