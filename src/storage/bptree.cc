#include "storage/bptree.h"

namespace udb {
BPTree::BPTree(const Options &options, const string &name)
    : env_(options.env_), db_file_(nullptr) {}

BPTree::~BPTree() {}

} // namespace udb