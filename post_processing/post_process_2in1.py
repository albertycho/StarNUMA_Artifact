import os
import subprocess

# Define your top directory and the list of benchmarks
top_dir = '/path/to/top_dir'  # Update this path
top_dir = '/scratch/acho44/NUMACXL/CS_RUNS/EX_DIR_HIER_dummy/'  # Update this path

# Predefined script paths - Update these
script1 = '/nethome/acho44/StarNUMA_Artifact/StarNUMA_Artifact/post_processing/extract_smarts_stat_per_benchmark.py'
script2 = '/nethome/acho44/StarNUMA_Artifact/StarNUMA_Artifact/post_processing/collect_smart_stats_top.py'
benchmarks = ["BFS", "CC", "SSSP", "FMI", "MASST", "TPCC", "POA"]



def run_script(script_name, working_dir):
    """Run the specified script in the given working directory."""
    try:
        subprocess.run(['python3', script_name], cwd=working_dir, check=True)
        print(f"Successfully ran {script_name} in {working_dir}")
    except subprocess.CalledProcessError as e:
        print(f"Error running {script_name} in {working_dir}: {e}")

def process_config_dirs():
    # Loop through each config directory
    for config_dir in os.listdir(top_dir):
        config_dir_path = os.path.join(top_dir, config_dir)

        if os.path.isdir(config_dir_path):
            print(f"Processing config directory: {config_dir}")

            # Loop through each benchmark directory
            for benchmark in benchmarks:
                benchmark_dir_path = os.path.join(config_dir_path, benchmark)

                if os.path.isdir(benchmark_dir_path):
                    # Run script1 in the benchmark directory
                    run_script(script1, benchmark_dir_path)

            # After all benchmarks have been processed, run script2 in the config directory
            run_script(script2, config_dir_path)

if __name__ == "__main__":
    process_config_dirs()
