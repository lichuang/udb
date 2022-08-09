#pragma once

#include "common/types.h"
#include "udb.h"

namespace udb {

class DBImpl : public Database {
public:
  DBImpl(const Options &options, const std::string &name);

  DBImpl(const Database &) = delete;
  DBImpl &operator=(const DBImpl &) = delete;

  virtual ~DBImpl() override;

  // Implementations of the Database interface
  virtual Status Write(const WriteOptions &options, const Slice &key,
                       const Slice &value);
  virtual Status Delete(const WriteOptions &options, const Slice &key);
  virtual Status Get(const ReadOptions &options, const Slice &key,
                     std::string *value);
  virtual Status Close(Database *);

  // Extra methods (for testing) that are not in the public Database interface

private:
  BPTree *tree;
}; // class Database
} // namespace udb