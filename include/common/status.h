#pragma once

#include "common/code.h"
#include "common/export.h"
#include <string>

namespace udb {

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

Code SaveErrorStatus(const Status &status);
Status GetErrorStatus();

} // namespace udb
