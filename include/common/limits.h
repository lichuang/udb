#pragma once

namespace udb {
// Max depth of the B-Tree. Any B-Tree deeper than this
// will be declared as corrupted.
static const int kTreeMaxDepth = 20;

}; // namespace udb