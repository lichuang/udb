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
udb_err_t error_report(udb_err_t err, int lineno, const char *type);

udb_err_t error_misuse(int);

#define UDB_MISUSE_BKPT error_misuse(__LINE__)

#endif /* _UDB_ERROR_H_ */