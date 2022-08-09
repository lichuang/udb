#pragma once

#include "common/export.h"
#include <string>

namespace udb {
class UDB_EXPORT Status {
public:
  bool ok() const { return code_ == kOK; }

private:
  enum Code {
    kOK = 0,
  };

  Code code_;
  std::string context_;
};
} // namespace udb
