#pragma once

#include "common/slice.h"
#include "common/status.h"
#include "common/types.h"

namespace udb {
class Cell;
class Cursor;
class Page;

// A page which has been loaded into memory.
class MemPage {
public:
  MemPage(Page *);

  MemPage(const MemPage &) = delete;
  MemPage &operator=(const MemPage &) = delete;

  int CellNumber() const { return cellNum_; }
  bool IsLeaf() const { return isLeaf_; }

  Status Search(const Slice &key, Cursor *, PageNo *);

  void ParseCell(Cursor *);

private:
  void ParseLeafPageCell(Cursor *);
  void ParseInternalPageCell(Cursor *);

private:
  int cellNum_;        // The number of cells
  bool isLeaf_;        // True if the page is a leaf page.
  unsigned char *data; // Pointer to disk image of the page data
};
}; // namespace udb