#include "udb_impl.h"

namespace udb {

DBImpl::DBImpl(const Options &options, const std::string &name) {}

DBImpl::~DBImpl() {}

Status DBImpl::Write(const WriteOptions &options, const Slice &key,
                     const Slice &value) {
  Status status;
  return status;
}

Status DBImpl::Delete(const WriteOptions &options, const Slice &key) {
  Status status;
  return status;
}

Status DBImpl::Get(const ReadOptions &options, const Slice &key,
                   std::string *value) {
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
  DBImpl *impl = new DBImpl(options, name);

  Status status;

  if (status.ok()) {
    *db = impl;
  } else {
    delete impl;
  }
  return status;
}
} // namespace udb