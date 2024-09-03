#!/bin/bash

# Get environment variables
GENOMICSBENCH_PATH=${GENOMICSBENCH_PATH:?Environment variable GENOMICSBENCH_PATH is not set.}
STARNUMA_ARTIFACT_PATH=${STARNUMA_ARTIFACT_PATH:?Environment variable STARNUMA_ARTIFACT_PATH is not set.}

# Copy files
cp "$STARNUMA_ARTIFACT_PATH/benchmarks/genomicsbench/fmi/fmi.cpp" "$GENOMICSBENCH_PATH/benchmarks/fmi/fmi.cpp"
cp "$STARNUMA_ARTIFACT_PATH/benchmarks/genomicsbench/poa/msa_spoa_omp.cpp" "$GENOMICSBENCH_PATH/benchmarks/poa/msa_spoa_omp.cpp"
echo 'copied files'

# Change directory to GENOMICSBENCH_PATH
cd "$GENOMICSBENCH_PATH" || exit

echo 'installing dependencies'
# Install dependencies
sudo apt-get install $(cat debian.prerequisites)

echo 'calling make'
# Run make
make

cd "$GENOMICSBENCH_PATH/benchmarks/fmi"
make
cd "../.."
cd "$GENOMICSBENCH_PATH/benchmarks/poa"
make
