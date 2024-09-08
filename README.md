# StarNUMA_Artifact

Before using/executing anything in this artifact, please set

export STARNUMA_ARTIFACT_PATH=your_cloned_path

## Benchmarks
Includes benchmarks used in the project. Compilation instructions are in the artifact document, or in the benchmark's README.

## Tracer
Includes source code and pintool for tracer binary, and scripts to run the tracer for each benchmark. 
Set 

export PIN_ROOT=your_path_to/StarNUMA_Artifacts/pin-3.30-98830-g1d7b601b3-gcc-linux

Additional required environment variables are described in tracer/README

## mem_profile
Tools to generate page allocation mapping from the traces. Page allocation mapping used for input to timing simulation.

## EX_DIR_HIER
Example directory hierarchy structure for timing simulation runs. Named 'example', but is designated as the directory to house timing simulations by default. I.e. provided scripts are tailored to use the given EX_DIR_HIER directory and its subdirs.

## DRAMsim3
Required for timing simulation in NUMA_Csim. Call 'make' inside DRAMsim3 to generate libdramsim3.so.

## NUMA_Csim
Timing simulation infrastructure based on champsim. Run build_all_bin.py to generate the required binaries for this project. Should be compiled after DRAMsim3 is compiled.

## timingsim_runscripts
Run setup_smarts_runs.py to create run_smarts.py (tailored to your environment via environment variables)  scripts inside EX_DIR_HIER.

## post_processing
extract_smarts_stat_per_benchmark.py can be run inside each smarts run suite (i.e. per each benchmark and configuration) to extract stats.
collect_smart_stats_top.py can be run for each configuration suite, once all benchmarks in the suite have completed and extract_smarts_stat_per_benchmark.py has been run.

If all benchmakrs finished, all post processing can be done with a single call to post_process_2in1.py

## plotting
Call each collect_figN_data.py and plot_figN.py in the directory you want to generate the plots.
