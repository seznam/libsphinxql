#!/bin/bash

set -e

# get dir same as the path of this script
DIR=$(dirname `readlink -f "$0"`)
PID=0

# path to the sphinx indexer
INDEXER=/bin/sphinx-indexer
SEARCHD=/bin/sphinx-searchd

function clean() {
    test $PID -ne 0 && kill $PID
    test -d index && rm -rf index/test.sp[a-z] && rmdir index
}

trap clean EXIT
mkdir -p index

# index prepared data
"$INDEXER" --all --config "$DIR/indexer.conf" < "$DIR/idx-data.xml"

# run searchd on background
"$SEARCHD" --console -c "$DIR/indexer.conf" &
PID=$!
sleep 0.2

./test-query
