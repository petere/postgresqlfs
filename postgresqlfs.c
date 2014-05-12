#include "debug.h"
#include "path.h"
#include "query.h"
#include "strlcpy.h"

#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define FUSE_USE_VERSION 27

#include <fuse/fuse.h>

#include <libpq-fe.h>


static PGconn *dbconn;
static time_t starttime;



static int
switch_database(struct dbpath *dbpath)
{
	PGconn *newdbconn;

	if (dbpath_is_root(*dbpath))
		return 1;

	if (strcmp(dbpath->database, PQdb(dbconn)) == 0)
		return 1;

	newdbconn = PQsetdbLogin(PQhost(dbconn),
							 PQport(dbconn),
							 PQoptions(dbconn),
							 PQtty(dbconn),
							 dbpath->database,
							 PQuser(dbconn),
							 PQpass(dbconn));

	if (PQstatus(newdbconn) != CONNECTION_OK)
	{
		debug("new connection failed");
		PQfinish(newdbconn);
		return 0;
	}

	PQfinish(dbconn);
	dbconn = newdbconn;
	return 1;
}


static int
postgresqlfs_getattr(const char *path, struct stat *buf)
{
	struct dbpath dbpath;

	buf->st_mode = 0;

	debug("path=%s", path);

	split_path(path, &dbpath);

	debug("%s", dbpath_to_string(&dbpath));

	if (!dbpath_is_database(dbpath) && !switch_database(&dbpath))
		return -EIO;

	if (!dbpath_exists(&dbpath, dbconn))
		return -ENOENT;

	buf->st_nlink = 1;
	buf->st_uid = getuid();
	buf->st_gid = getgid();

	if (dbpath_is_column(dbpath))
		buf->st_mode |= S_IFREG | 0444;
	else
		buf->st_mode |= S_IFDIR | 0755;

	buf->st_atime = starttime;
	buf->st_mtime = starttime;
	buf->st_ctime = starttime;

	buf->st_blksize = 1;

	if (dbpath_is_column(dbpath))
	{
		PGresult *res;

		res = db_query(dbconn,
					   "SELECT octet_length(CAST(%s AS text)) FROM %s.%s WHERE ctid = '%s';",
					   dbpath.column + 3, dbpath.schema, dbpath.table, rowname_to_ctid(dbpath.row));
		if (!res)
			return -EIO;

		buf->st_size = atol(PQgetvalue(res, 0, 0));

		PQclear(res);
	}
	else
	{
		buf->st_size = 42;
	}

	buf->st_blocks = buf->st_size;

	return 0;
}


static int
postgresqlfs_mkdir(const char *path, mode_t mode __attribute__((unused)))
{
	struct dbpath dbpath;

	split_path(path, &dbpath);

	// TODO: use mode sensibly

	if (dbpath_is_root(dbpath))
		return -EEXIST;
	else if (dbpath_is_database(dbpath))
		return db_command(dbconn, "CREATE DATABASE %s;", dbpath.database);
	else {
		/* Note: Don't switch the database when creating a database. */
		if (!switch_database(&dbpath))
			return -EIO;

		if (dbpath_is_schema(dbpath))
			return db_command(dbconn, "CREATE SCHEMA %s;", dbpath.schema);
		else if (dbpath_is_table(dbpath))
			return -ENOSYS; /* maybe: create zero column table */
		else if (dbpath_is_row(dbpath))
			return -ENOSYS; /* maybe: translate to INSERT with primary key and default values? */
		else
			return -ENOENT;
	}
}


static int
postgresqlfs_unlink(const char *path)
{
	struct dbpath dbpath;

	split_path(path, &dbpath);

	return -EPERM;
}


static int
postgresqlfs_rmdir(const char *path)
{
	struct dbpath dbpath;

	split_path(path, &dbpath);

	if (!switch_database(&dbpath))
		return -EIO;

	if (dbpath_is_root(dbpath))
		return -EBUSY;
	else if (dbpath_is_database(dbpath))
		return -ENOTEMPTY;
	else if (dbpath_is_schema(dbpath))
		return db_command(dbconn, "DROP SCHEMA %s;", dbpath.schema);
	else if (dbpath_is_table(dbpath))
		return -ENOSYS; /* maybe: drop table if it has zero columns */
	else if (dbpath_is_row(dbpath))
		return -ENOSYS; /* maybe: remove row by ctid if table has zero columns */
	else
		return -ENOTDIR;
}


static int
postgresqlfs_rename(const char *oldpath, const char *newpath)
{
	struct dbpath olddbpath, newdbpath;

	split_path(oldpath, &olddbpath);
	split_path(newpath, &newdbpath);

	if (!dbpaths_are_same_level(&olddbpath, &newdbpath))
		return -EXDEV;

	if (dbpath_is_root(olddbpath))
		return -EINVAL;
	else if (dbpath_is_database(olddbpath))
	{
		// XXX check whether paths exist; clearer error codes
		return db_command(dbconn,
						  "ALTER DATABASE %s RENAME TO %s;",
						  olddbpath.database, newdbpath.database);
	}
	else if (dbpath_is_schema(olddbpath))
	{
		/* moving a schema to a different database is not possible */
		if (strcmp(olddbpath.database, newdbpath.database) != 0)
			return -EXDEV;

		return db_command(dbconn,
						  "ALTER SCHEMA %s RENAME TO %s;",
						  olddbpath.schema, newdbpath.schema);
	}
	else if (dbpath_is_table(olddbpath))
	{
		/* moving a table to a different database is not possible */
		if (strcmp(olddbpath.database, newdbpath.database) != 0)
			return -EXDEV;

		if (strcmp(olddbpath.schema, newdbpath.schema) != 0)
			return -ENOSYS; // XXX for the time being

		return db_command(dbconn,
						  "ALTER TABLE %s.%s RENAME TO %s;",
						  olddbpath.schema, olddbpath.table, newdbpath.table);
	}
	else if (dbpath_is_row(olddbpath))
		return -EINVAL; // XXX right code?
	else if (dbpath_is_column(olddbpath))
	{
		/* renaming a column to reappear in a different table doesn't make sense */
		if (strcmp(olddbpath.database, newdbpath.database) != 0
			|| strcmp(olddbpath.schema, newdbpath.schema) != 0
			|| strcmp(olddbpath.table, newdbpath.table) != 0
			|| strcmp(olddbpath.row, newdbpath.row) != 0)
			return -EINVAL;

		/* column number must stay the same (could be changed if
		 * PotgreSQL supported reordering) */
		if (strncmp(olddbpath.column, newdbpath.column, 3) != 0)
			return -EINVAL;

		return db_command(dbconn,
						  "ALTER TABLE %s.%s RENAME COLUMN %s TO %s;",
						  olddbpath.schema, olddbpath.table, olddbpath.column + 3, newdbpath.column + 3);
	}

	return -EINVAL;
}


static int
postgresqlfs_chmod(const char *path, mode_t mode)
{
	debug("chmod %s %u", path, mode);
	return -ENOSYS;
}


static int
postgresqlfs_read(const char *path, char *buf, size_t size, off_t offset,
				  struct fuse_file_info *fi __attribute__((unused)))
{
	struct dbpath dbpath;

	split_path(path, &dbpath);

	if (!switch_database(&dbpath))
		return -EIO;

	if (!dbpath_exists(&dbpath, dbconn))
		return -ENOENT;

	if (dbpath_is_column(dbpath))
	{
		PGresult *res;
		size_t len;
		const char *val;

		res = db_query(dbconn,
					   "SELECT substring(CAST(%s AS text) FROM %jd FOR %zu) FROM %s.%s WHERE ctid = '%s';",
					   dbpath.column + 3, (intmax_t) (offset + 1), size, dbpath.schema, dbpath.table, rowname_to_ctid(dbpath.row));
		if (!res)
			return -EIO;

		val = PQgetvalue(res, 0, 0);
		len = strlen(val);

		debug("value \"%s\" len=%zu", val, len);
		memcpy(buf, val, len);

		PQclear(res);

		return (int) len;
	}
	else
		return -EISDIR;
}


static int
postgresqlfs_write(const char *path, const char *buf, size_t size, off_t offset,
				   struct fuse_file_info *fi __attribute__((unused)))
{
	struct dbpath dbpath;

	split_path(path, &dbpath);

	if (!switch_database(&dbpath))
		return -EIO;

	if (!dbpath_exists(&dbpath, dbconn))
		return -ENOENT;

	if (dbpath_is_column(dbpath))
	{
		int res;

		res = db_command(dbconn,
						 "UPDATE %s.%s SET %s = overlay(CAST(%s AS text) PLACING '%s' FROM %jd FOR %zu) WHERE ctid = '%s';",
						 dbpath.schema, dbpath.table,
						 dbpath.column + 3, dbpath.column + 3, buf, (intmax_t) (offset + 1), size,
						 rowname_to_ctid(dbpath.row));
		if (!res)
			return res;
		return (int) size;
	}
	else
		return -EISDIR;
}


static int
postgresqlfs_readdir(const char * path, void *buf, fuse_fill_dir_t filler,
					 off_t offset __attribute__((unused)),
					 struct fuse_file_info *fi __attribute__((unused)))
{
	struct dbpath dbpath;

	debug("path=%s", path);

	split_path(path, &dbpath);

	debug("%s", dbpath_to_string(&dbpath));

	if (!switch_database(&dbpath))
		return -EIO;

	if (!dbpath_exists(&dbpath, dbconn))
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	if (dbpath_is_root(dbpath))
	{
		PGresult *res;
		int i;

		res = db_query(dbconn, "SELECT datname FROM pg_database WHERE datallowconn;");
		if (!res)
			return -EIO;
		for (i = 0; i < PQntuples(res); i++)
		{
			debug("added database %s", PQgetvalue(res, i, 0));
			filler(buf, PQgetvalue(res, i, 0), NULL, 0);
		}
		PQclear(res);
	}
	else if (dbpath_is_database(dbpath))
	{
		PGresult *res;
		int i;

		res = db_query(dbconn, "SELECT nspname FROM pg_namespace;");
		if (!res)
			return -EIO;
		for (i = 0; i < PQntuples(res); i++)
		{
			debug("added schema %s", PQgetvalue(res, i, 0));
			filler(buf, PQgetvalue(res, i, 0), NULL, 0);
		}
		PQclear(res);
	}
	else if (dbpath_is_schema(dbpath))
	{
		PGresult *res;
		int i;

		res = db_query(dbconn, "SELECT relname FROM pg_class c, pg_namespace n WHERE c.relnamespace = n.oid AND relkind IN ('r', 'v') AND n.nspname = '%s';", dbpath.schema);
		if (!res)
			return -EIO;
		for (i = 0; i < PQntuples(res); i++)
		{
			debug("added table %s", PQgetvalue(res, i, 0));
			filler(buf, PQgetvalue(res, i, 0), NULL, 0);
		}
		PQclear(res);
	}
	else if (dbpath_is_table(dbpath))
	{
		PGresult *res;
		int i;
		char str[PATH_MAX];

		str[0] = '\0';
		res = db_query(dbconn, "SELECT attname FROM pg_attribute WHERE attrelid = (SELECT ci.oid FROM pg_class ci, pg_class ct, pg_index i, pg_namespace n WHERE n.oid = ct.relnamespace AND ci.oid = i.indexrelid AND ct.oid = i.indrelid AND n.nspname = '%s' AND ct.relname = '%s' AND NOT EXISTS (SELECT 1 FROM pg_attribute at, pg_attribute ai WHERE at.attrelid = ct.oid AND ai.attrelid = ci.oid AND at.attname = ai.attname AND at.attnum < 0) AND i.indisunique ORDER BY i.indisprimary DESC LIMIT 1) ORDER BY attnum;", dbpath.schema, dbpath.table);
		for (i = 0; i < PQntuples(res); i++)
		{
			strcat(str, PQgetvalue(res, i, 0));
			strcat(str, "::text || '_' || ");
		}
		debug("unique key is %s", str);
		PQclear(res);

		res = db_query(dbconn, "SELECT %s ctid FROM %s.%s;", str, dbpath.schema, dbpath.table);
		if (!res)
			return -EIO;
		for (i = 0; i < PQntuples(res); i++)
		{
			debug("added row %s", PQgetvalue(res, i, 0));
			filler(buf, PQgetvalue(res, i, 0), NULL, 0);
		}
		PQclear(res);
	}
	else if (dbpath_is_row(dbpath))
	{
		PGresult *res;
		int i;

		res = db_query(dbconn, "SELECT to_char(attnum, 'FM00') || '_' || attname FROM pg_attribute a, pg_class c, pg_namespace n WHERE a.attrelid = c.oid AND c.relnamespace = n.oid AND NOT attisdropped AND attnum > 0 AND n.nspname = '%s' AND c.relname = '%s';", dbpath.schema, dbpath.table);
		if (!res)
			return -EIO;
		for (i = 0; i < PQntuples(res); i++)
		{
			debug("added column %s", PQgetvalue(res, i, 0));
			filler(buf, PQgetvalue(res, i, 0), NULL, 0);
		}
		PQclear(res);
	}
	else
		return -ENOTDIR;

	return 0;
}


static struct fuse_operations postgresqlfs_ops = {
	.getattr = postgresqlfs_getattr,
	.mkdir = postgresqlfs_mkdir,
	.unlink = postgresqlfs_unlink,
	.rmdir = postgresqlfs_rmdir,
	.rename = postgresqlfs_rename,
	.chmod = postgresqlfs_chmod,
	.chown = NULL,			// TODO
	.truncate = NULL,		// TODO
	.read = postgresqlfs_read,
	.write = postgresqlfs_write,
	.readdir = postgresqlfs_readdir
};


int
main(int argc, char *argv[])
{
	int status;

	starttime = time(NULL);

	dbconn = PQsetdbLogin(NULL, NULL, NULL, NULL, "postgres", NULL, NULL);
	if (PQstatus(dbconn) != CONNECTION_OK)
	{
		debug("connection bad: %s", PQerrorMessage(dbconn));
		return 1;
	}

	status = fuse_main(argc, argv, &postgresqlfs_ops, NULL);

	PQfinish(dbconn);

	return status;
}
