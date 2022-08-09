#pragma once

#include "common/export.h"

namespace udb {
class BPTree;

// Cursor of b+tree
struct UDB_EXPORT BptCursor {
  BPTree *tree;
};
} // namespace udb