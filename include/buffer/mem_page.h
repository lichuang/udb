#pragma once

#include "common/slice.h"
#include "common/status.h"
#include "common/types.h"

namespace udb {
class Cursor;

// A page which has been loaded into memory.
class MemPage {
public:
  int CellNumber() const { return cellNum_; }
  bool IsLeaf() const { return isLeaf_; }

  Status Search(const Slice &key, Cursor *, PageNo *);

private:
  int cellNum_; // The number of cells
  bool isLeaf_; // True if the page is a leaf page.
};
}; // namespace udb