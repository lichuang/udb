

#include "buffer/mem_page.h"
#include "common/debug.h"
#include "common/int.h"
#include "common/string.h"
#include "storage/cell.h"
#include "storage/cursor.h"
#include "storage/page.h"
#include "storage/page_layout.h"

namespace udb {

MemPage::MemPage() {}

Code MemPage::InitFromPage(Page *page) {
  PageNo pageNo = page->DiskPageNo();
  unsigned char *data = page->Data();

  // If this page 1 then read the file header.
  if (pageNo == 1) {
    headerOffset_ = kPage1HeaderOffset;
  } else {
    headerOffset_ = 0;
  }

  // Read page header
  Code code = ReadPageHeader(data, pageNo);
  if (code != kOk) {
    return code;
  }

  return code;
}

// Search the key in the page.
// If not reached the leaf page, return child page no in pageNo and kOk.
// Return error otherwise.
Code MemPage::Search(const Slice &key, Cursor *cursor, PageNo *pageNo) {
  Cell cell;
  Code code;

  *pageNo = kInvalidPageNo;

  // Compare with the lower and upper bound of the page.
  code = GetCell(0, &cell);
  Assert(cell->IsLeafPageCell() == isLeaf_);
  if (key <= cell) {
  }

  // Binary search for the key
  return kOk;
}

Code MemPage::ReadPageHeader(unsigned char *data, PageNo pageNo) {
  char flag;

  flag = data[headerOffset_ + kPageFlagHeaderOffset];
  if (flag != kInternalPage && flag != kLeafPage) {
    return SaveErrorStatus(
        Status(kCorrupt, FormatString("wrong page flag for page {}", pageNo)));
  }

  cellNum_ = get2byte(&data[headerOffset_ + kCellNumberHeaderOffset]);
  if (cellNum_ < 0) {
    return SaveErrorStatus(Status(
        kCorrupt, FormatString("wrong cell number for page {}", pageNo)));
  }
  if (flag == kLeafPage) {
    isLeaf_ = true;
    headerSize_ = kLeafPageHeaderSize;
  } else {
    isLeaf_ = false;
    headerSize_ = kInternalPageHeaderSize;
  }

  return kOk;
}

Code MemPage::GetCell(int i, Cell *cell) {
  Assert(i >= 0 && i < cellNum_);
  Assert(cell->IsInvalid());
  unsigned char *cellPtrAry = &(data_[kCellPtrOffet]);
  unsigned char *cellContent;

  return cell->ParseFrom(cellContent);
}

void MemPage::ParseCell(Cursor *cursor) {
  int cellIndex = cursor->CellIndex();

  // Do some sanity checking.
  Assert(cursor->isValid());
  Assert(cellIndex >= 0 && cellIndex < cellNum_);

  if (isLeaf_) {
    ParseLeafPageCell(cursor);
  } else {
    ParseInternalPageCell(cursor);
  }
}

void MemPage::ParseLeafPageCell(Cursor *cursor) {}

void MemPage::ParseInternalPageCell(Cursor *cursor) {}

} // namespace udb
