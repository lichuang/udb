#ifndef _UDB_ERROR_H_
#define _UDB_ERROR_H_

#include "udb.h"

/*
** The SQLITE_*_BKPT macros are substitutes for the error codes with
** the same name but without the _BKPT suffix.  These macros invoke
** routines that report the line-number on which the error originated
** using sqlite3_log().  The routines also provide a convenient place
** to set a debugger breakpoint.
*/
udb_code_t error_report(udb_code_t err, int lineno, const char *type);

udb_code_t err_corrupt(int);

udb_code_t error_misuse(int);

udb_code_t error_cantopen(int);

#define UDB_MISUSE_BKPT error_misuse(__LINE__)

#define UDB_CORRUPT_BKPT err_corrupt(__LINE__)

#define UDB_CANTOPEN_BKPT error_cantopen(__LINE__)

#endif /* _UDB_ERROR_H_ */