#!/bin/bash

set -e

# path to the sphinx indexer
INDEXER=/bin/sphinx-indexer

mkdir -p index
"$INDEXER" --all --config indexer.conf < idx-data.xml
