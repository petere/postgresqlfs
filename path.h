#ifndef PATH_H
#define PATH_H

#include <limits.h>
#include <stdbool.h>

struct dbpath {
	char database[PATH_MAX];
	char schema[PATH_MAX];
	char table[PATH_MAX];
	char row[PATH_MAX];
	char column[PATH_MAX];
};


#define dbpath_component_is_valid(obj, comp) (((obj).comp)[0] != '\0')

#define dbpath_is_root(obj)     (!dbpath_component_is_valid(obj, database))
#define dbpath_is_database(obj) (!dbpath_component_is_valid(obj, schema))
#define dbpath_is_schema(obj)   (!dbpath_component_is_valid(obj, table))
#define dbpath_is_table(obj)    (!dbpath_component_is_valid(obj, row))
#define dbpath_is_row(obj)      (!dbpath_component_is_valid(obj, column))
#define dbpath_is_column(obj)   (dbpath_component_is_valid(obj, column))

extern int split_path(const char *in, struct dbpath *out);
extern const char *dbpath_to_string(const struct dbpath *in);
extern bool dbpaths_are_same_level(struct dbpath *path1, struct dbpath *path2);

#endif /* PATH_H */
