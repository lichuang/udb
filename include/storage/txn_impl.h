#pragma once

#include "common/code.h"
#include "common/limits.h"
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
  Code FillInCell(const Slice &key, const Slice &value, char *cell,
                  int *cellSize);

public:
  bool write_;
  int lockIndex_;
  Cursor *cursor_;
  char tmpSpace[kPageSize];
};
} // namespace udb