#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

OUTPUT="$SCRIPT_DIR/hypecc"
SOURCE="$SCRIPT_DIR/src/main.cpp"

CXX="${CXX:-g++}"
CXXFLAGS="-std=c++20 -O3 -march=native -Wall -Wextra"
LDFLAGS="-lcurl"

echo "Derleniyor: $SOURCE -> $OUTPUT"

if $CXX $CXXFLAGS "$SOURCE" -o "$OUTPUT" $LDFLAGS; then
    echo "Compilation successful: $OUTPUT"
else
    echo "Compilation failed."
    exit 1
fi
