## Compiling Tracer
Set
export PIN_ROOT=your_path_to/StarNUMA_Artifacts/pin-3.30-98830-g1d7b601b3-gcc-linux.
Inside pin_tracer, call 
make obj-intel64/combined_tracer.so

## Environment Variables:
• SNUMA_PINPATH: path to the pin binary
• SNUMA_TRACER_PATH: Path to the compiled combined tracer.so
• SNUMA_GRAPH_PATH: This is just for graph benchmarks.
Path to your generated graph.
• SNUMA_GAPBS_PATH: This is just for graph benchmarks.
Path to gapbs directory.
For non graph benchmarks, environment variable for path to
each benchamrk and input file must be set.
• GENOMICS_INPUT_PATH: path to input dataset for ge-
nomicsbench.
• FMI_PATH: path to FMI binary.
• POA_PATH: path to POA binary.
• MASSTREE_PATH: path to masstree binary.
• MASSTREE_PATH: path to masstree binary.
• TPCC_PATH: path to tpcc binary.


## Tracer outputs:
• champsim_[i]_[j].trace where i is the thread-id and j is the
phase number, where a phase is defined by 1 billion instruc-
tions. These files are instructions traces used in timing simula-
tion. Once tracing completes, Instruction traces must be xz
compressed for the timing simulator to recognize them.
• memtrace_t[i].out where i is the thread-id. These traces log
cache-filtered memory accesses of each thread throughout the
entire run. These files are read by the memory profiling tool to
generate the page allocation mapping.
• mtrace_t[i]_[j].out where i is the thread-id and j is the phase
number. These traces log (non-cache-filtered) memory accesses
just for the regions sampled for timing simulation. They are
used to generate memory traffic from cores to model contention
at the CXL pool and socket interconnect.
