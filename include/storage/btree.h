#pragma once

#include "common/slice.h"
#include "common/types.h"
#include "storage/txn_impl.h"

namespace udb {

class BTree {
public:
  BTree(PageNo root, const std::string &name);

  BTree(const BTree &) = delete;
  BTree &operator=(const BTree &) = delete;

  ~BTree();

  Status Write(TxnImpl *, const Slice &key, const Slice &value);

  Status Delete(TxnImpl *, const Slice &key);

  Status Get(TxnImpl *, const Slice &key, Slice *value);

  PageNo Root() const { return root_; }

private:
  PageNo root_;
}; // class Database
} // namespace udb