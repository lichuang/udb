#pragma once

#include "common/export.h"
#include <string>

namespace udb {
// Code for operation results.
enum UDB_EXPORT Code {
  kOk = 0,
  kCorrupt = 1,
};

} // namespace udb