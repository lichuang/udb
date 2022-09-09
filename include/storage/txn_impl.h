#pragma once

#include "storage/udb_impl.h"

namespace udb {

class Cursor;

class TxnImpl : public Txn {
public:
  TxnImpl(bool write, int lockIndex);

  TxnImpl() = default;

  TxnImpl(const TxnImpl &) = delete;
  TxnImpl &operator=(const TxnImpl &) = delete;

  virtual ~TxnImpl() override;

  virtual Status OpenTree(const std::string &name, BTree **,
                          bool createIfNotExists) override;

  virtual Status DeleteTree(const std::string &name) override;

  virtual Status Write(BTree *, const Slice &key, const Slice &value) override;

  virtual Status Delete(BTree *, const Slice &key) override;

  virtual Status Get(BTree *, const Slice &key, Slice *value) override;

  int LockIndex() const { return lockIndex_; }

private:
public:
  bool write_;
  int lockIndex_;
  Cursor *cursor_;
};
} // namespace udb