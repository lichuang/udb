#include "storage/cursor.h"
#include "storage/tree.h"

namespace udb {
Cursor::Cursor(const Slice &key, Tree *tree)
    : key_(key), tree_(tree), root_(tree->RootPage()), page_(nullptr) {}

Cursor::~Cursor() {}
} // namespace udb