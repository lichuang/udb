#pragma once

#include "common/export.h"
#include "common/slice.h"
#include "common/types.h"

namespace udb {
class Tree;

const static int kCursorMaxDepth = 20;

// Cursor of b+tree
class UDB_EXPORT Cursor {
public:
  Cursor(const Slice &key, Tree *tree);

  Cursor(const Cursor &) = delete;
  Cursor &operator=(const Cursor &) = delete;

  ~Cursor();

private:
  Slice key_;
  Tree *tree_;
  PageNo root_;
  Page *page_;
  Page *stack_[kCursorMaxDepth];
};
} // namespace udb