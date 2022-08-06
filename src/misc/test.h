#ifndef _UDB_TEST_H_
#define _UDB_TEST_H_

#define testcase(x)

/*
** The ALWAYS and NEVER macros surround boolean expressions which
** are intended to always be true or false, respectively.  Such
** expressions could be omitted from the code completely.  But they
** are included in a few cases in order to enhance the resilience
** of SQLite to unexpected behavior - to make the code "self-healing"
** or "ductile" rather than being "brittle" and crashing at the first
** hint of unplanned behavior.
**
** In other words, ALWAYS and NEVER are added for defensive code.
**
** When doing coverage testing ALWAYS and NEVER are hard-coded to
** be true and false so that the unreachable code they specify will
** not be counted as untested code.
*/
#if defined(UDB_COVERAGE_TEST) || defined(UDB_MUTATION_TEST)
#define ALWAYS(X) (1)
#define NEVER(X) (0)
#elif !defined(NDEBUG)
#define ALWAYS(X) ((X) ? 1 : (assert(0), 0))
#define NEVER(X) ((X) ? (assert(0), 1) : 0)
#else
#define ALWAYS(X) (X)
#define NEVER(X) (X)
#endif

#endif /* _UDB_TEST_H_ */