

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

Status MemPage::InitFromPage(Page *page) {
  PageNo pageNo = page->DiskPageNo();
  unsigned char *data = page->Data();

  // If this page 1 then read the file header.
  if (pageNo == 1) {
    headerOffset_ = kPage1HeaderOffset;
  }

  // Read page header
  ReadPageHeader(data, pageNo);
  if (status_.Ok()) {
    return status_;
  }

  return status_;
}

void MemPage::ReadPageHeader(unsigned char *data, PageNo pageNo) {
  char flag;

  flag = data[headerOffset_ + kPageFlagHeaderOffset];
  if (flag != kInternalPage && flag != kLeafPage) {
    status_ =
        Status(kCorrupt, FormatString("wrong page flag for page {}", pageNo));
  }

  cellNum_ = get2byte(&data[headerOffset_ + kCellNumberHeaderOffset]);
  if (cellNum_ < 0) {
    status_ =
        Status(kCorrupt, FormatString("wrong cell number for page {}", pageNo));
  }
  isLeaf_ = flag == kLeafPage;
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
