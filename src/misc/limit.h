#ifndef _UDB_LIMIT_H_
#define _UDB_LIMIT_H_

/* Maximum page size.  The upper bound on this value is 65536.  This a limit
** imposed by the use of 16-bit offsets within each page.
**
** Earlier versions of SQLite allowed the user to change this value at
** compile time. This is no longer permitted, on the grounds that it creates
** a library that is technically incompatible with an SQLite library
** compiled with a different limit. If a process operating on a database
** with a page-size of 65536 bytes crashes, then an instance of SQLite
** compiled with the default page-size limit will not be able to rollback
** the aborted transaction. This could lead to database corruption.
*/
const static int UDB_MAX_PAGE_SIZE = 65536;
const static int UDB_MIN_PAGE_SIZE = 512;

#endif /* _UDB_LIMIT_H_ */