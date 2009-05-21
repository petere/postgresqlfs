#ifndef QUERY_H
#define QUERY_H

#include "path.h"

#include <libpq-fe.h>

extern PGresult *db_query(PGconn *dbconn, const char *stmt, ...) __attribute__((format(printf, 2, 3)));
extern int db_row_exists(PGconn *dbconn, const char *stmt, ...) __attribute__((format(printf, 2, 3)));
extern int dbpath_exists(const struct dbpath *dbpath, PGconn *dbconn);
extern const char *rowname_to_ctid(const char *row);

#endif /* QUERY_H */
