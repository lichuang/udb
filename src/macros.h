#ifndef _UDB_MACROS_H_
#define _UDB_MACROS_H_

#include <stddef.h>

#include "limit.h"

#define UNUSED_PARAMETER(x) (void)(x)

/*
 ** A valide page size MUST be: power of 2, and between [512,65536]
 */
#define VALID_PAGE_SIZE(sz)                                                    \
  ((sz & (sz - 1)) == 0 && sz >= UDB_MIN_PAGE_SIZE && sz <= UDB_MAX_PAGE_SIZE)

/*
** Round up a number to the next larger multiple of 8.  This is used
** to force 8-byte alignment on 64-bit architectures.
*/
#define ROUND8(x) (((x) + 7) & ~7)

/*
** The PTR_WITHIN(P,S,E) macro checks to see if pointer P points to
** something between S (inclusive) and E (exclusive).
**
** In other words, S is a buffer and E is a pointer to the first byte after
** the end of buffer S.  This macro returns true if P points to something
** contained within the buffer S.
*/
#define PTR_WITHIN(P, S, E)                                                    \
  (((uintptr_t)(P) >= (uintptr_t)(S)) && ((uintptr_t)(P) < (uintptr_t)(E)))

/*
** GCC does not define the offsetof() macro so we'll have to do it
** ourselves.
*/
#ifndef offsetof
#define offsetof(STRUCTURE, FIELD) ((int)((char *)&((STRUCTURE *)0)->FIELD))
#endif

/*
** Macros to determine whether the machine is big or little endian,
** and whether or not that determination is run-time or compile-time.
**
** For best performance, an attempt is made to guess at the byte-order
** using C-preprocessor macros.  If that is unsuccessful, or if
** -DUDB_BYTEORDER=0 is set, then byte-order is determined
** at run-time.
*/
#ifndef UDB_BYTEORDER
#if defined(i386) || defined(__i386__) || defined(_M_IX86) ||                  \
    defined(__x86_64) || defined(__x86_64__) || defined(_M_X64) ||             \
    defined(_M_AMD64) || defined(_M_ARM) || defined(__x86) ||                  \
    defined(__ARMEL__) || defined(__AARCH64EL__) || defined(_M_ARM64)
#define UDB_BYTEORDER 1234
#elif defined(sparc) || defined(__ppc__) || defined(__ARMEB__) ||              \
    defined(__AARCH64EB__)
#define UDB_BYTEORDER 4321
#else
#define UDB_BYTEORDER 0
#endif
#endif
#if UDB_BYTEORDER == 4321
#define UDB_BIGENDIAN 1
#define UDB_LITTLEENDIAN 0
#define UDB_UTF16NATIVE UDB_UTF16BE
#elif UDB_BYTEORDER == 1234
#define UDB_BIGENDIAN 0
#define UDB_LITTLEENDIAN 1
#define UDB_UTF16NATIVE UDB_UTF16LE
#else
#ifdef UDB_AMALGAMATION
const int sqlite3one = 1;
#else
extern const int sqlite3one;
#endif
#define UDB_BIGENDIAN (*(char *)(&sqlite3one) == 0)
#define UDB_LITTLEENDIAN (*(char *)(&sqlite3one) == 1)
#define UDB_UTF16NATIVE (UDB_BIGENDIAN ? UDB_UTF16BE : UDB_UTF16LE)
#endif

#endif /* _UDB_MACROS_H_ */