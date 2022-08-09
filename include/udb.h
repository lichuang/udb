#pragma once

#include <string>

#include "common/export.h"
#include "common/options.h"
#include "common/slice.h"
#include "common/status.h"

namespace udb {

// Update CMakeLists.txt if you change these
static const int kMajorVersion = 0;
static const int kMinorVersion = 1;

class UDB_EXPORT Database {
  static Status Open(const Options &options, const std::string &name,
                     Database **);

  Database() = default;

  Database(const Database &) = delete;
  Database &operator=(const Database &) = delete;

  virtual ~Database();

  // Write the database entry for "key" to "value".
  // Returns OK on success, and a non-OK status on error.
  virtual Status Write(const WriteOptions &options, const Slice &key,
                       const Slice &value) = 0;

  // Remove the database entry (if any) for "key".  Returns OK on
  // success, and a non-OK status on error.
  virtual Status Delete(const WriteOptions &options, const Slice &key) = 0;

  // If the database contains an entry for "key" store the
  // corresponding value in *value and return OK.
  virtual Status Get(const ReadOptions &options, const Slice &key,
                     std::string *value) = 0;

  // Close the database, Returns OK on success.
  virtual Status Close(Database *) = 0;
}; // class Database
} // namespace udb