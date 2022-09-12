#pragma once

#include <stdint.h>

/* The layout of a page(copy mostly from sqlite btree model)
 **
 ** Each b+tree pages is divided into three sections:  The header, the
 ** cell pointer array, and the cell content area.  Page 1 also has a 100-byte
 ** file header that occurs before the page header.
 **
 **      |----------------|
 **      | file header    |   100 bytes.  Page 1 only.
 **      |----------------|
 **      | page header    |   8 bytes for leaves.  12 bytes for interior nodes
 **      |----------------|
 **      | cell pointer   |   |  2 bytes per cell.  Sorted order.
 **      | array          |   |  Grows downward
 **      |                |   v
 **      |----------------|
 **      | unallocated    |
 **      | space          |
 **      |----------------|   ^  Grows upwards
 **      | cell content   |   |  Arbitrary order interspersed with freeblocks.
 **      | area           |   |  and free space fragments.
 **      |----------------|
 **
 ** The page headers looks like this:
 **
 **   OFFSET   SIZE     DESCRIPTION
 **      0       1      Flags. 1: internal-page, 2: leaf-page
 **      1       2      byte offset to the first freeblock
 **      3       2      number of cells on this page
 **      5       2      first byte of the cell content area
 **      7       1      number of fragmented free bytes
 **      8       4      Right child (the Ptr(N) value).  Omitted on leaves.
 **
 ** The flags define the format of this b+tree page.  The internal-page flag
 ** means that this page carries only keys and no data.
 ** The leaf-page flag means that this page has no children.
 **
 ** The cell pointer array begins on the first byte after the page header.
 ** The cell pointer array contains zero or more 2-byte numbers which are
 ** offsets from the beginning of the page to the cell content in the cell
 ** content area. The cell pointers occur in sorted order.  The system strives
 ** to keep free space after the last cell pointer so that new cells can
 ** be easily added without having to defragment the page.
 **
 ** Cell content is stored at the very end of the page and grows toward the
 ** beginning of the page.
 **
 ** Unused space within the cell content area is collected into a linked list of
 ** freeblocks.  Each freeblock is at least 4 bytes in size.  The byte offset
 ** to the first freeblock is given in the header.  Freeblocks occur in
 ** increasing order.  Because a freeblock must be at least 4 bytes in size,
 ** any group of 3 or fewer unused bytes in the cell content area cannot
 ** exist on the freeblock chain.  A group of 3 or fewer free bytes is called
 ** a fragment.  The total number of bytes in all fragments is recorded.
 ** in the page header at offset 7.
 **
 **    SIZE    DESCRIPTION
 **      2     Byte offset of the next freeblock
 **      2     Bytes in this freeblock
 **
 ** Cells are of variable length.  Cells are stored in the cell content area at
 ** the end of the page.  Pointers to the cells are in the cell pointer array
 ** that immediately follows the page header.  Cells is not necessarily
 ** contiguous or in order, but cell pointers are contiguous and in order.
 **
 ** Cell content makes use of variable length integers.  A variable
 ** length integer is 1 to 9 bytes where the lower 7 bits of each
 ** byte are used.  The integer consists of all bytes that have bit 8 set and
 ** the first byte with bit 8 clear.  The most significant byte of the integer
 ** appears first.  A variable-length integer may not be more than 9 bytes long.
 ** As a special case, all 8 bytes of the 9th byte are used as data.  This
 ** allows a 64-bit integer to be encoded in 9 bytes.
 **
 **    0x00                      becomes  0x00000000
 **    0x7f                      becomes  0x0000007f
 **    0x81 0x00                 becomes  0x00000080
 **    0x82 0x00                 becomes  0x00000100
 **    0x80 0x7f                 becomes  0x0000007f
 **    0x8a 0x91 0xd1 0xac 0x78  becomes  0x12345678
 **    0x81 0x81 0x81 0x81 0x01  becomes  0x10204081
 **
 ** Variable length integers are used for holding the number of
 ** bytes of key and data in a btree cell.
 **
 ** The content of a cell looks like this:
 **
 **    SIZE    DESCRIPTION
 **      4     Page number of the left child. Omitted if leaf page flag is set.
 **     var    Number of bytes of data. Omitted if the internal flag is set.
 **     var    Number of bytes of key.
 **      *     Payload
 **      4     First page of the overflow chain.  Omitted if no overflow
 **
 ** Overflow pages form a linked list.  Each page except the last is completely
 ** filled with data (pagesize - 4 bytes).  The last page can have as little
 ** as 1 byte of data.
 **
 **    SIZE    DESCRIPTION
 **      4     Page number of next overflow page
 **      *     Data
 **
 ** Freelist pages come in two subtypes: trunk pages and leaf pages.  The
 ** file header points to the first in a linked list of trunk page.  Each trunk
 ** page points to multiple leaf pages.  The content of a leaf page is
 ** unspecified.  A trunk page looks like this:
 **
 **    SIZE    DESCRIPTION
 **      4     Page number of next trunk page
 **      4     Number of leaf pointers on this page
 **      *     zero or more pages numbers of leaves
 */

namespace udb {
// Page 1 header offset
static const uint8_t kPage1HeaderOffset = 100;

// Page header field offsets
static const uint8_t kPageFlagHeaderOffset = 0;
static const uint8_t kFirstFreeblockHeaderOffset = 1;
static const uint8_t kCellNumberHeaderOffset = 3;
static const uint8_t kCellContentHeaderOffset = 5;

// Page flags
static const char kInternalPage = 1;
static const char kLeafPage = 2;
} // namespace udb