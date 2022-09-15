#pragma once

#include "common/export.h"
#include <string>

namespace udb {
// Code for operation results.
enum UDB_EXPORT Code {
  kOk = 0,

  // The database file has been corrupted.
  kCorrupt = 1,

  // Cursor has reached the kTreeMaxDepth
  kCursorOverflow = 2,
};

} // namespace udb