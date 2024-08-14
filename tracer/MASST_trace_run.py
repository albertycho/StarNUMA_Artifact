#!/usr/bin/env python

import os
import sys
import argparse
import random


pinpath = os.getenv('SNUMAPINPATH', 'YOU_NEED_TO_SET_IT_WITH_EXPORT')
#'~/pin_330/pin-3.30-98830-g1d7b601b3-gcc-linux/pin'
tracerpath= os.getenv('SNUMA_TRACER_PATH', 'YOU_NEED_TO_SET_IT_WITH_EXPORT')
graph_path= os.getenv('SNUMA_GRAPH_PATH', 'YOU_NEED_TO_SET_IT_WITH_EXPORT')
gapbs_path= os.getenv('SNUMA_GAPBS_PATH', 'YOU_NEED_TO_SET_IT_WITH_EXPORT')

#TODO update this
masstree_path="path/to/bin/masstree"

masstree_prefix = 'BENCH_QPS=2000 TBENCH_MAXREQS=5000000 '                      
masstree_cmd = masstree_path+' -j66 mycsba masstree'
                                                                                
trace_instsM = 150                                                              
trace_insts = trace_instsM*1000000                                              
prog_cmd = masstree_cmd                                                         
cmd =masstree_prefix + pinpath+' -t '+tracerpath+' -startFF -t '+str(trace_insts)+ ' -- '+prog_cmd


print(cmd)
with open('trace_insts'+str(trace_instsM)+'M.txt', 'w') as f:
    f.write("trace "+str(trace_instsM)+"Million insts per phase\n")
os.system(cmd)
