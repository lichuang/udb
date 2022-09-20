#pragma once

#include "common/slice.h"
#include "common/status.h"
#include "common/types.h"
#include "storage/storage_types.h"

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

  Code InitFromPage(Page *);

  PageNo MemPageNo() const { return pageNo_; }
  int CellNumber() const { return cellNum_; }
  bool IsLeaf() const { return isLeaf_; }

  // Search the key in the page.
  // If not reached the leaf page, return child page no in pageNo and kOk.
  // Return error otherwise.
  Code Search(const Slice &key, Cursor *, PageNo *, CursorLocation *,
              int *cellIndex);

  void ParseCell(Cursor *);

  int FreeSpace() const;

private:
  Code ReadPageHeader(char *data, PageNo pageNo);
  void ParseLeafPageCell(Cursor *);
  void ParseInternalPageCell(Cursor *);

  // Return the i-th cell info.
  Code GetCell(int i, Cell *);

private:
  Page *page_;
  PageNo pageNo_;
  uint16_t headerOffset_; // 100 for page 1.  0 otherwise
  uint16_t headerSize_;   // 12 bytes for internal-page, 8 bytes for leaf page.
  int cellNum_;           // The number of cells
  bool isLeaf_;           // True if the page is a leaf page.
  char *data_;            // Pointer to disk image of the page data
};
}; // namespace udb