#!/bin/sh
# Run from the repository root: bash test/test.sh
set -e
./test/test_nasa.sh
./test/test_non_nasa.sh
