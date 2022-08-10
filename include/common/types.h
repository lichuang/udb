#pragma once

#include <string>

#include "common/export.h"

using namespace std;
namespace udb {
class BufferManager;

class Os;
class File;

class Tree;
class Cursor;

/*
** The type used to represent a page number.  The first page in a file
** is called page 1.  0 is used to represent "not a page".
*/
typedef uint32_t PageNo;
static const PageNo kInvalidPageNo = 0;

class Page;

} // namespace udb