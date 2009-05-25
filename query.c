#define _GNU_SOURCE

#include "debug.h"
#include "query.h"
#include "path.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libpq-fe.h>


static PGresult *
db_query_v(PGconn *dbconn, const char *stmt, va_list ap)
{
	PGresult *res;
	char *buf;

	vasprintf(&buf, stmt, ap);

	res = PQexec(dbconn, buf);
	free(buf);
	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		debug("query status was %s", PQresStatus(PQresultStatus(res)));
		PQclear(res);
		return NULL;
	}

	return res;
}


PGresult *
db_query(PGconn *dbconn, const char *stmt, ...)
{
	PGresult *res;
	va_list ap;

	va_start(ap, stmt);
	res = db_query_v(dbconn, stmt, ap);
	va_end(ap);

	return res;
}


static int
db_command_v(PGconn *dbconn, const char *stmt, va_list ap)
{
	PGresult *res;
	char *buf;

	vasprintf(&buf, stmt, ap);

	res = PQexec(dbconn, buf);
	free(buf);
	// TODO: communicate the SQLSTATE via some global variable back to errno
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		debug("command status was %s", PQresStatus(PQresultStatus(res)));
		PQclear(res);
		return -EIO;
	}

	return 0;
}


int
db_command(PGconn *dbconn, const char *stmt, ...)
{
	int res;
	va_list ap;

	va_start(ap, stmt);
	res = db_command_v(dbconn, stmt, ap);
	va_end(ap);

	return res;
}


int
db_row_exists(PGconn *dbconn, const char *stmt, ...)
{
	PGresult *res;
	int n;
	va_list ap;

	va_start(ap, stmt);
	res = db_query_v(dbconn, stmt, ap);
	va_end(ap);

	n = PQntuples(res);
	PQclear(res);

	return n > 0;
}


int
dbpath_exists(const struct dbpath *dbpath, PGconn *dbconn)
{
	if (dbpath_is_root(*dbpath))
		return 1;

	if (dbpath_is_database(*dbpath))
		return db_row_exists(dbconn, "SELECT 1 FROM pg_database WHERE datname = '%s';", dbpath->database);

	if (dbpath_is_schema(*dbpath))
		return db_row_exists(dbconn, "SELECT 1 FROM pg_namespace WHERE nspname = '%s';", dbpath->schema);

	if (dbpath_is_table(*dbpath))
		return db_row_exists(dbconn, "SELECT 1 FROM pg_namespace n, pg_class c WHERE n.oid = c.relnamespace AND nspname = '%s' AND relname = '%s';", dbpath->schema, dbpath->table);

	if (dbpath_is_row(*dbpath))
		return db_row_exists(dbconn, "SELECT 1 FROM %s.%s WHERE ctid = '%s';", dbpath->schema, dbpath->table, rowname_to_ctid(dbpath->row));

	if (dbpath_is_column(*dbpath))
		return db_row_exists(dbconn, "SELECT 1 FROM pg_namespace n, pg_class c, pg_attribute a WHERE n.oid = c.relnamespace AND c.oid = a.attrelid AND nspname = '%s' AND relname = '%s' AND to_char(attnum, 'FM00') || '_' || attname = '%s';", dbpath->schema, dbpath->table, dbpath->column);

	return 0;
}


const char *
rowname_to_ctid(const char *row)
{
	return strrchr(row, '(');
}
