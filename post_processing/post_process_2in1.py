import os
import subprocess

starnuma_artifact_path = os.getenv('STARNUMA_ARTIFACT_PATH')

# Define your top directory and the list of benchmarks
top_dir = starnuma_artifact_path+'EX_DIR_HIER'

# Predefined script paths - Update these
script1 = starnuma_artifact_path+ '/post_processing/extract_smarts_stat_per_benchmark.py'
script2 = starnuma_artifact_path+ '/post_processing/collect_smart_stats_top.py'
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
