

#include "buffer/mem_page.h"
#include "common/bytes.h"
#include "common/debug.h"
#include "common/string.h"
#include "storage/cell.h"
#include "storage/cursor.h"
#include "storage/page.h"
#include "storage/page_layout.h"

namespace udb {

MemPage::MemPage() {}

Code MemPage::InitFromPage(Page *page) {
  PageNo pageNo = page->DiskPageNo();
  char *data = page->Data();

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

  data_ = data;
  return code;
}

// Search the key in the page.
// If not reached the leaf page, return child page no in pageNo and kOk.
// Return error otherwise.
Code MemPage::Search(const Slice &key, Cursor *cursor, PageNo *pageNo,
                     CursorLocation *location) {
  Cell cell;
  Code code;
  int compare;

  *pageNo = kInvalidPageNo;

  // Fast path: compare with the low and high cell.

  // Compare with the low cell of the page.
  code = GetCell(0, &cell);
  Assert(cell->IsLeafPageCell() == isLeaf_);
  compare = key.Compare(cell.Key(), cell.KeySize());
  if (compare <= 0) {
    // key is not bigger than low bound, move to left child of first cell.
    *pageNo = cell.LeftChild();
    *location = (compare == 0) ? Equal : Left;
    return kOk;
  }

  // Compare with the upper cell of the page.
  code = GetCell(cellNum_ - 1, &cell);
  Assert(cell->IsLeafPageCell() == isLeaf_);
  compare = key.Compare(cell.Key(), cell.KeySize());
  if (compare == 0) {
    // Equal to up bound, move to the left child of last cell.
    *pageNo = cell.LeftChild();
    *location = Equal;
    return kOk;
  }

  if (compare > 0) {
    // bigger than up bound, move to right child of the page.
    *pageNo = Get4Byte(&data_[headerOffset_ + kRightChildPageNoHeaderOffset]);
    *location = Right;
    return kOk;
  }

  // Now that the key is between (low, high)

  // Binary search for the key
  int low = 0;
  int high = cellNum_ - 1;
  int mid;
  while (low <= high) {
    mid = (high + low) / 2;
    code = GetCell(mid, &cell);
    Assert(cell->IsLeafPageCell() == isLeaf_);
    compare = key.Compare(cell.Key(), cell.KeySize());
    if (compare == 0) {
      *pageNo = cell.LeftChild();
      *location = Equal;
      return kOk;
    } else if (compare < 0) {
      low = mid + 1;
    } else {
      high = mid - 1;
    }

    if (low == cellNum_ - 1 || high == 0) {
      break;
    }
  }

  Assert(compare != 0);
  Assert(low == cellNum_ - 1 && compare < 0);
  Assert(high == 0 && compare > 0);
  *pageNo = cell.LeftChild();
  *location = (compare > 0) ? Left : Right;

  return kOk;
}

Code MemPage::ReadPageHeader(char *data, PageNo pageNo) {
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
