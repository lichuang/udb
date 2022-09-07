#pragma once

#include <string>

#include "common/export.h"
#include "common/slice.h"
#include "common/status.h"

namespace udb {

// Update CMakeLists.txt if you change these
static const int kMajorVersion = 0;
static const int kMinorVersion = 1;

class BTree;
class Txn;

struct UDB_EXPORT Options {
public:
  // Create an Options object with default values for all fields.
  Options();

  // page size, MUST be a power of 2 and between [1024, 65536]
  int pageSize_ = 4096;

  // cache size
  int cacheSize_ = 1024000;
};

class UDB_EXPORT Database {
public:
  static Status Open(const Options &options, const std::string &name,
                     Database **);

  Database() = default;

  Database(const Database &) = delete;
  Database &operator=(const Database &) = delete;

  virtual ~Database();

  // Begin a transaction.
  virtual Txn *Begin(bool write) = 0;

  // Commit a transaction.
  virtual Status Commit(Txn *) = 0;

  // Close the database, Returns OK on success.
  virtual Status Close(Database *) = 0;
}; // class Database

class Txn {
public:
  Txn() = default;

  Txn(const Txn &) = delete;
  Txn &operator=(const Txn &) = delete;

  virtual ~Txn();

  // Open a tree by name, return BTree if exist.
  // When createIfNotExists is true, create the tree if not exist.
  virtual Status OpenTree(const std::string &name, BTree **,
                          bool createIfNotExists) = 0;

  // Delete a tree by name.
  // Note that in a transaction, if operate a BTree after
  // it has been deleted, will return error.
  virtual Status DeleteTree(const std::string &name);

  // Write the BTree entry for "key" to "value".
  // If Btree is None, write entry in default BTree of db.
  // Returns OK on success, and a non-OK status on error.
  virtual Status Write(BTree *, const Slice &key, const Slice &value) = 0;

  // Remove the BTree entry (if any) for "key".
  // If Btree is None, write entry in default BTree of db.
  // Returns OK on success, and a non-OK status on error.
  virtual Status Delete(BTree *, const Slice &key) = 0;

  // If the BTree contains an entry for "key" store the
  // corresponding value in value and return OK.
  virtual Status Get(BTree *, const Slice &key, Slice *value) = 0;
}; // class Txn
} // namespace udb