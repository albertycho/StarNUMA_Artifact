#!/usr/bin/env python

import os
import sys
import argparse
import random


##TOMODIFY - your pinpath and tracer path
pinpath = '~/pin_330/pin-3.30-98830-g1d7b601b3-gcc-linux/pin'
tracerpath='/TOMODIFY/obj-intel64/combined_tracer_40B_dbg.so'


### TOMODIFY - replace with path to benchmark
graph_path = '/TOMODIFY/gapbs/benchmark/kron_30_32/kron_30_32.sg'
bfs_cmd = '/TOMODIFY/gapbs/bfs -f '+graph_path+' - 20'


prog_cmd = bfs_cmd

trace_instsM = 150
trace_insts = trace_instsM*1000000

## do not use -startFF unless benchmark is modified with ROI magic instruction
cmd = pinpath+' -t '+tracerpath+' -startFF -t '+str(trace_insts)+ ' -- '+prog_cmd

print(cmd)
with open('trace_insts'+str(trace_instsM)+'M.txt', 'w') as f:
    f.write("trace "+str(trace_instsM)+"Million insts per phase\n")
os.system(cmd)
