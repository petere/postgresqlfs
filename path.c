#include "path.h"

#include "strlcpy.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>


#define dbpath_invalidate_component(obj, comp) (((obj).comp)[0] = '\0')


int
split_path(const char *in, struct dbpath *out)
{
	const char *pos1;
	const char *pos2 = in;

	if (in[0] != '/')
		return -EINVAL;

	dbpath_invalidate_component(*out, database);
	dbpath_invalidate_component(*out, schema);
	dbpath_invalidate_component(*out, table);
	dbpath_invalidate_component(*out, row);
	dbpath_invalidate_component(*out, column);

	/* root */

	if (in[1] == '\0')
		return 0;

	/* database */

	pos1 = pos2 + 1;
	pos2 = strchr(pos1, '/');

	if (pos2 == NULL)
	{
		strcpy(out->database, pos1);
		return 0;
	}
	strlcpy(out->database, pos1, pos2 - pos1);

	/* schema */

	pos1 = pos2 + 1;
	pos2 = strchr(pos1, '/');

	if (pos2 == NULL)
	{
		strcpy(out->schema, pos1);
		return 0;
	}
	strlcpy(out->schema, pos1, pos2 - pos1);

	/* table */

	pos1 = pos2 + 1;
	pos2 = strchr(pos1, '/');

	if (pos2 == NULL)
	{
		strcpy(out->table, pos1);
		return 0;
	}
	strlcpy(out->table, pos1, pos2 - pos1);

	/* row */

	pos1 = pos2 + 1;
	pos2 = strchr(pos1, '/');

	if (pos2 == NULL)
	{
		strcpy(out->row, pos1);
		return 0;
	}
	strlcpy(out->row, pos1, pos2 - pos1);

	/* column */

	pos1 = pos2 + 1;
	pos2 = strchr(pos1, '/');

	if (pos2 == NULL)
	{
		strcpy(out->column, pos1);
		return 0;
	}
	strlcpy(out->column, pos1, pos2 - pos1);

	/* rest */

	pos1 = pos2 + 1;
	pos2 = strchr(pos1, '/');

	if (pos2 == NULL)
		return 0;
	else
		return -EINVAL;
}


const char *
dbpath_to_string(const struct dbpath *in)
{
	static char out[PATH_MAX * 5 + 50];

	snprintf(out, sizeof(out),
			 "database=%s schema=%s table=%s row=%s column=%s",
			 in->database,
			 in->schema,
			 in->table,
			 in->row,
			 in->column);

	return out;
}
