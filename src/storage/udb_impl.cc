#include "storage/udb_impl.h"
#include "storage/btree.h"
#include "storage/txn_impl.h"

namespace udb {

DBImpl::DBImpl(const Options &options, const std::string &path) {}

DBImpl::~DBImpl() {}

Txn *DBImpl::Begin(bool write) {
  int lockIndex = Lock(write);
  return new TxnImpl(write, lockIndex);
}

Status DBImpl::Commit(Txn *txn) {
  Status status;
  return status;
}

Status DBImpl::Close(Database *) {
  Status status;
  return status;
}

int DBImpl::Lock(bool write) { return 0; }

void Unlock(int lockIndex) {}

Database::~Database() = default;

Status Database::Open(const Options &options, const std::string &name,
                      Database **db) {
  *db = nullptr;
  DBImpl *tree = new DBImpl(options, name);

  Status status;

  if (status.Ok()) {
    *db = tree;
  } else {
    delete tree;
  }
  return status;
}
} // namespace udb