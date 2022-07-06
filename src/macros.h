#ifndef _UDB_MACROS_H_
#define _UDB_MACROS_H_

#define UNUSED_PARAMETER(x) (void)(x)

/*
 ** A valide page size MUST be: power of 2, and between [512,65536]
 */
#define VALID_PAGE_SIZE(sz) ((sz & (sz - 1)) == 0 && sz >= 512 && sz <= 65536)

/*
** Round up a number to the next larger multiple of 8.  This is used
** to force 8-byte alignment on 64-bit architectures.
*/
#define ROUND8(x) (((x) + 7) & ~7)

#endif /* _UDB_MACROS_H_ */