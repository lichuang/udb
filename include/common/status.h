#pragma once

#include "common/export.h"
#include <string>

namespace udb {
// Code for operation results.
enum UDB_EXPORT Code {
  kOk = 0,
  kCorrupt = 1,
};

class UDB_EXPORT Status {
public:
  Status() : code_(kOk), context_("") {}
  Status(Code code, const std::string &context)
      : code_(code), context_(context) {}

  // Intentionally copyable.
  Status(const Status &) = default;
  Status &operator=(const Status &) = default;

  bool Ok() const { return code_ == kOk; }

private:
  Code code_;
  std::string context_;
};

} // namespace udb
