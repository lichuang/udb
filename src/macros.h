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

#endif /* _UDB_MACROS_H_ */