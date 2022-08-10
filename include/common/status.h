#pragma once

#include "common/export.h"
#include <string>

namespace udb {

class UDB_EXPORT Status {
public:
  Status() : code_(kOk), context_("") {}
  bool Ok() const { return code_ == kOk; }

private:
  enum Code {
    kOk = 0,
  };
  Code code_;
  std::string context_;
};
} // namespace udb
