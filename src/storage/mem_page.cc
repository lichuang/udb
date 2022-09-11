#include "buffer/mem_page.h"
#include "common/debug.h"
#include "storage/cell.h"
#include "storage/cursor.h"

namespace udb {
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
