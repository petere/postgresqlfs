postgresqlfs
============

postgresqlfs is a FUSE driver to access PostgreSQL databases as a file
system.  It exposes the object structure of a PostgreSQL database
instance as directories and files that you can operate on using file
system tools.

The idea is that you can use this together with Midnight Commander to
browse and edit a database in text mode in situations where psql would
be to cumbersome but pgAdmin or phpPgAdmin are not available.

Use `postgresqlfs --help` to get usage information.  Usually, it is just

    $ postgresqlfs mountpoint

to mount, and

    $ fusermount -u mountpoint

to unmount.

You also need access to the `/dev/fuse` device, which may be tied to
some group membership on your operating system.

[![Build Status](https://secure.travis-ci.org/petere/postgresqlfs.png)](http://travis-ci.org/petere/postgresqlfs)

Peter Eisentraut <peter@eisentraut.org>
