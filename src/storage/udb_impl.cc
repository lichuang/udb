#include "storage/udb_impl.h"
#include "storage/btree.h"
#include "storage/txn_impl.h"

namespace udb {

DBImpl::DBImpl(const Options &options, const std::string &path) {}

DBImpl::~DBImpl() {}

Txn *DBImpl::Begin(bool write) { return new TxnImpl(this, write); }

Status DBImpl::Commit(Txn *) {
  Status status;
  return status;
}

Status DBImpl::Close(Database *) {
  Status status;
  return status;
}

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