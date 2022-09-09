#pragma once

#include "common/types.h"
#include "udb.h"
#include <map>

namespace udb {

class BTree;

class DBImpl : public Database {
public:
  DBImpl(const Options &options, const std::string &name);

  DBImpl(const DBImpl &) = delete;
  DBImpl &operator=(const DBImpl &) = delete;

  virtual ~DBImpl() override;

  // Implementations of the Database interface
  virtual Txn *Begin(bool write) override;

  virtual Status Commit(Txn *) override;

  // Close the database, Returns OK on success.
  virtual Status Close(Database *) override;

  static DBImpl *Instance();

private:
  // Lock and return the index.
  int Lock(bool write);

  void Unlock(int lockIndex);

private:
  std::map<std::string, BTree *> tree_map_;
  BTree *default_tree_;
}; // class Database

#define DBInstance DBImpl::Instance()

} // namespace udb