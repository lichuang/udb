#pragma once

#include "common/export.h"
#include "common/status.h"
#include "common/types.h"

namespace udb {
class UDB_EXPORT Page {
public:
  char *Data() { return data_; }
  PageNo DiskPageNo() const { return pageNo_; }

private:
  char *data_;
  PageNo pageNo_;
};
} // namespace udb