#!/usr/bin/env python

import os
import sys
import argparse
import random

pinpath = '/PATH_TO_PIN/pin'
tracerpath='/PATH_TO_TRACER/obj-intel64/combined_tracer.so'


masstree_prefix = 'BENCH_QPS=2000 TBENCH_MAXREQS=5000000 '
masstree_cmd = '/PATH_TO_MASSTREE/masstree/mttest_integrated -j66 mycsba masstree'
masstree_cmd = '/PATH_TO_MASSTREE/masstree/mttest_integrated_10M_vallen100 -j66 mycsba masstree'

trace_instsM = 150
trace_insts = trace_instsM*1000000

prog_cmd = masstree_cmd
cmd =masstree_prefix + pinpath+' -t '+tracerpath+' -startFF -t '+str(trace_insts)+ ' -- '+prog_cmd

with open('trace_insts'+str(trace_instsM)+'M.txt', 'w') as f:
    f.write("trace "+str(trace_instsM)+"Million insts per phase\n")
os.system(cmd)
