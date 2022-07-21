#include <stdio.h>

#include "global.h"
#include "misc/error.h"

udb_err_t error_report(udb_err_t err, int lineno, const char *type) {
  return err;
}

udb_err_t err_corrupt(int lineno) {
  return error_report(UDB_CORRUPT, lineno, "database corruption");
}

int error_misuse(int lineno) {
  return error_report(UDB_MISUSE, lineno, "misuse");
}