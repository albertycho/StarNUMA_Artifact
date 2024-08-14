import os
import pandas as pd

# Get the top directory from the environment variable
top_dir = os.getenv('STARNUMA_RUNS_TOP_DIR')

if top_dir is None:
    raise EnvironmentError("Environment variable STARNUMA_RUNS_TOP_DIR is not set.")

# Define directories
baseline_dir = os.path.join(top_dir, "baseline_main")
starnuma_dir = os.path.join(top_dir, "StarNUMA_main")
static_alloc_dir = os.path.join(top_dir, "STATIC_ALLOC")

# Define file name
smart_stats_file = "collected_smart_stats.txt"

def combine_and_create_stats(baseline_main_file, starnuma_main_file, static_alloc_file, output_file):
    # Load baseline_main, StarNUMA_main, and STATIC_ALLOC data
    baseline_main_df = pd.read_csv(baseline_main_file, skipinitialspace=True)
    starnuma_main_df = pd.read_csv(starnuma_main_file, skipinitialspace=True)
    static_alloc_df = pd.read_csv(static_alloc_file, skipinitialspace=True)
    
    # Initialize a new DataFrame for the combined data
    combined_df = pd.DataFrame()

    # Combine the data
    combined_df['benchmark'] = baseline_main_df['benchmark']
    combined_df['base_IPC_main'] = baseline_main_df['base_IPC']
    combined_df['base_IPC'] = static_alloc_df['base_IPC']
    combined_df['cxi_IPC_main'] = starnuma_main_df['cxi_IPC']
    combined_df['cxi_IPC'] = static_alloc_df['cxi_IPC']
    
    # Save the combined dataframe
    combined_df.to_csv(output_file, index=False)
    print(f"Combined file saved as: {output_file}")

def main():
    # Define paths to the files
    baseline_main_smart_stats_path = os.path.join(baseline_dir, smart_stats_file)
    starnuma_main_smart_stats_path = os.path.join(starnuma_dir, smart_stats_file)
    static_alloc_smart_stats_path = os.path.join(static_alloc_dir, smart_stats_file)

    # Define output file name
    combined_smart_stats_output = os.path.join(os.getcwd(), "collected_smart_stats.txt")

    # Combine and create stats file
    combine_and_create_stats(
        baseline_main_smart_stats_path,
        starnuma_main_smart_stats_path,
        static_alloc_smart_stats_path,
        combined_smart_stats_output
    )

if __name__ == "__main__":
    main()
