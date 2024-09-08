#!/usr/bin/env python

import os
import sys
import argparse
import random


pinpath = os.getenv('SNUMA_PINPATH', 'YOU_NEED_TO_SET_IT_WITH_EXPORT')
#'~/pin_330/pin-3.30-98830-g1d7b601b3-gcc-linux/pin'
tracerpath= os.getenv('SNUMA_TRACER_PATH', 'YOU_NEED_TO_SET_IT_WITH_EXPORT')
graph_path= os.getenv('SNUMA_GRAPH_PATH', 'YOU_NEED_TO_SET_IT_WITH_EXPORT')
gapbs_path= os.getenv('SNUMA_GAPBS_PATH', 'YOU_NEED_TO_SET_IT_WITH_EXPORT')
poa_path = os.getenv('POA_PATH', 'YOU_NEED_TO_SET_IT_WITH_EXPORT')
input_path= os.getenv('GENOMICS_INPUT_PATH', 'YOU_NEED_TO_SET_IT_WITH_EXPORT')

poa_input_path=input_path+"/poa/large/input.fasta"

# Get the environment variable
starnuma_artifact_path = os.getenv('STARNUMA_ARTIFACT_PATH')

# Construct the path you want to change to
target_directory = os.path.join(starnuma_artifact_path, 'tracer', 'TRACES', 'POA')

# Change to the target directory
os.chdir(target_directory)

print(f"Changed directory to: {os.getcwd()}")



poa_cmd = poa_path+" -s "+poa_input_path+" -t 64"


prog_cmd = poa_cmd

trace_instsM = 150
trace_insts = trace_instsM*1000000

## do not use -startFF unless benchmark is modified with ROI magic instruction
cmd = pinpath+' -t '+tracerpath+' -startFF -t '+str(trace_insts)+ ' -- '+prog_cmd

print(cmd)
with open('trace_insts'+str(trace_instsM)+'M.txt', 'w') as f:
    f.write("trace "+str(trace_instsM)+"Million insts per phase\n")
os.system(cmd)
