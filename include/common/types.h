#pragma once

#include <string>

#include "common/export.h"

using namespace std;
namespace udb {
class BufferManager;

class Env;
class File;

class BPTree;
class BptCursor;

typedef uint32_t PageNo;
class Page;

} // namespace udb