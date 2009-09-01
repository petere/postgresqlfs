#!/bin/bash

. tap-functions

notokx() {
    local command="$@"

    local line=
    diag "Output of '$command':"
    $command 2>&1 | while read line ; do
        diag "$line"
    done
    ok $((!${PIPESTATUS[0]})) "$command"
}


setup() {
    export PGOPTIONS='--client-min-messages=warning'
    psql -d postgres -c 'CREATE DATABASE postgresqlfs_test'
    createlang plpgsql postgresqlfs_test
    psql -d postgresqlfs_test -f testdata.sql
    mkdir mnt
    ../postgresqlfs mnt
}

cleanup() {
    if [ -d mnt ]; then
	fusermount -q -u mnt
	rmdir mnt
    fi
    export PGOPTIONS='--client-min-messages=warning'
    psql -d postgres -c 'DROP DATABASE IF EXISTS postgresqlfs_test'
    psql -d postgres -c 'DROP DATABASE IF EXISTS postgresqlfs_test2'
}

trap 'cleanup' EXIT

set -e
cleanup
setup
set +e


plan_tests 21


## getattr + readdir

okx ls mnt/postgresqlfs_test
okx ls mnt/postgresqlfs_test/test
is $(ls mnt/postgresqlfs_test/test | wc -l) 1
okx ls mnt/postgresqlfs_test/test/test1
is $(ls mnt/postgresqlfs_test/test/test1 | wc -l) 3
okx ls mnt/postgresqlfs_test/test/test1/1_*
is $(ls mnt/postgresqlfs_test/test/test1/1_* | wc -l) 2
okx ls mnt/postgresqlfs_test/test/test1/1_*/01_a
okx ls mnt/postgresqlfs_test/test/test1/1_*/02_b


## read

is $(cat mnt/postgresqlfs_test/test/test1/1_*/01_a) "1"
is $(cat mnt/postgresqlfs_test/test/test1/1_*/02_b) "one"
is $(cat mnt/postgresqlfs_test/test/test1/2_*/01_a) "2"
is $(cat mnt/postgresqlfs_test/test/test1/2_*/02_b) "two"


## mkdir

okx mkdir mnt/postgresqlfs_test2
psql -l | grep -q postgresqlfs_test2
ok $?

okx mkdir mnt/postgresqlfs_test2/testschema
is $(psql -d postgresqlfs_test2 -At -c "SELECT count(*) FROM pg_namespace WHERE nspname = 'testschema'") 1

notokx mkdir mnt/postgresqlfs_test2/testschema/newtable


## rmdir

okx rmdir mnt/postgresqlfs_test2/testschema
is $(psql -d postgresqlfs_test2 -At -c "SELECT count(*) FROM pg_namespace WHERE nspname = 'testschema'") 0

notokx rmdir mnt/postgresqlfs_test2


## rename

# TODO
