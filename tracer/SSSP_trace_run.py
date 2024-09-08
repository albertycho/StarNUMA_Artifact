#!/usr/bin/env python

import os
import sys
import argparse
import random


##TOMODIFY - your pinpath and tracer path
pinpath = os.getenv('SNUMA_PINPATH', 'YOU_NEED_TO_SET_IT_WITH_EXPORT')
#'~/pin_330/pin-3.30-98830-g1d7b601b3-gcc-linux/pin'
tracerpath= os.getenv('SNUMA_TRACER_PATH', 'YOU_NEED_TO_SET_IT_WITH_EXPORT')
#graph_path= os.getenv('SNUMA_GRAPH_PATH', 'YOU_NEED_TO_SET_IT_WITH_EXPORT')
gapbs_path= os.getenv('SNUMA_GAPBS_PATH', 'YOU_NEED_TO_SET_IT_WITH_EXPORT')
#wsg_graph_path= os.getenv('WSG_GRAPH_PATH', 'YOU_NEED_TO_SET_IT_WITH_EXPORT')
wsg_graph_path= gapbs_path+'/benchmark/graphs/kron.wsg'
# Get the environment variable
starnuma_artifact_path = os.getenv('STARNUMA_ARTIFACT_PATH')

# Construct the path you want to change to
target_directory = os.path.join(starnuma_artifact_path, 'tracer', 'TRACES', 'SSSP')

# Change to the target directory
os.chdir(target_directory)

print(f"Changed directory to: {os.getcwd()}")



### TOMODIFY - replace with path to benchmark
sssp_cmd = gapbs_path+'/sssp -f '+wsg_graph_path+' -n 5'


prog_cmd = sssp_cmd

trace_instsM = 150
trace_insts = trace_instsM*1000000

## do not use -startFF unless benchmark is modified with ROI magic instruction
cmd = pinpath+' -t '+tracerpath+' -startFF -t '+str(trace_insts)+ ' -- '+prog_cmd

print(cmd)
with open('trace_insts'+str(trace_instsM)+'M.txt', 'w') as f:
    f.write("trace "+str(trace_instsM)+"Million insts per phase\n")
os.system(cmd)
