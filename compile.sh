#!/bin/bash

set -e

OUTPUT="hypecc"

SOURCES=(
    "src/main.cpp"
    "src/core/engine.cpp"
    "src/utils/network.cpp"
)

CXXFLAGS="-std=c++20 -Isrc -O3 -march=native"
LDFLAGS="-lcurl"

if g++ $CXXFLAGS "${SOURCES[@]}" -o "$OUTPUT" $LDFLAGS; then
    echo "Compilation successful: ./$OUTPUT"
else
    echo "Compilation failed."
    exit 1
fi
