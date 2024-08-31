#!/bin/bash

# Create the build directory
rm -r build
mkdir -p build

# Change to the build directory
cd build

# Run cmake with the specified options
cmake .. -DLOG_LEVEL=0 -DCMAKE_BUILD_TYPE=Release -DCC_ALG=SILO -DBENCHMARK=TPCC

# Build the project with make
make -j

