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
  MemPage();

  MemPage(const MemPage &) = delete;
  MemPage &operator=(const MemPage &) = delete;

  Status InitFromPage(Page *);

  PageNo MemPageNo() const { return pageNo_; }
  int CellNumber() const { return cellNum_; }
  bool IsLeaf() const { return isLeaf_; }

  Status Search(const Slice &key, Cursor *, PageNo *);

  void ParseCell(Cursor *);

private:
  void ReadPageHeader(unsigned char *data, PageNo pageNo);
  void ParseLeafPageCell(Cursor *);
  void ParseInternalPageCell(Cursor *);

private:
  Page *page_;
  PageNo pageNo_;
  uint8_t headerOffset_; // 100 for page 1.  0 otherwise
  int cellNum_;          // The number of cells
  bool isLeaf_;          // True if the page is a leaf page.
  unsigned char *data_;  // Pointer to disk image of the page data
  Status status_;
};
}; // namespace udb