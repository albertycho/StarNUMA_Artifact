#
# Copyright (C) 2023-2023 Intel Corporation.
# SPDX-License-Identifier: MIT
#

#
# This script runs all the performance tests and prints out
# in formatted tables the results.
# 32 bit and 64 bit tables go to output file k#.txt where #=32/64
#

make test TARGET=ia32 DEBUG=1
make test TARGET=intel64 DEBUG=1
python goperf.py --debug > k32.txt
python goperf.py --debug --tar intel64 > k64.txt

