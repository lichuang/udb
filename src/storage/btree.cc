#include "storage/btree.h"

namespace udb {

BTree::BTree(const Options &options, const std::string &name) {}

BTree::~BTree() {}

Txn *BTree::Begin(bool write) { return nullptr; }

Status BTree::Write(Txn *txn, const Slice &key, const Slice &value) {
  Status status;
  return status;
}

Status BTree::Delete(Txn *txn, const Slice &key) {
  Status status;
  return status;
}

Status BTree::Get(Txn *txn, const Slice &key, std::string *value) {
  Status status;
  return status;
}

Status BTree::Commit(Txn *) {
  Status status;
  return status;
}

} // namespace udb