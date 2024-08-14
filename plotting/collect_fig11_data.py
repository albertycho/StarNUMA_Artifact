import os
import pandas as pd
import shutil

# Get the top directory from the environment variable
top_dir = os.getenv('STARNUMA_RUNS_TOP_DIR')

if top_dir is None:
    raise EnvironmentError("Environment variable STARNUMA_RUNS_TOP_DIR is not set.")

# Define directories
baseline_dir = os.path.join(top_dir, "baseline_main")
starnuma_dir = os.path.join(top_dir, "StarNUMA_main")

# Define file names
smart_stats_file = "collected_smart_stats.txt"
hop_stats_file = "collected_hop_stats.txt"

# Define additional directories for copying and renaming
additional_dirs = {
    "StarNUMA_halfBW": "SNUMA_halfBW_collected_smart_stats.txt",
    "baseline_2XBW": "b_2X_collected_smart_stats.txt",
    "baseline_ISOBW": "b_ISOBW_collected_smart_stats.txt"
}

def combine_files(baseline_file, starnuma_file, output_file):
    # Load baseline and StarNUMA data
    baseline_df = pd.read_csv(baseline_file)
    starnuma_df = pd.read_csv(starnuma_file)
    
    # Initialize a new DataFrame for the combined data
    combined_df = baseline_df.copy()

    # Iterate over columns and replace 'cxi_' columns in the combined dataframe with values from starnuma_df
    for col in combined_df.columns:
        if 'cxi_' in col:
            combined_df[col] = starnuma_df[col]
    
    # Save the combined dataframe
    combined_df.to_csv(output_file, index=False)
    print(f"Combined file saved as: {output_file}")

def copy_and_rename_files():
    for dir_name, new_filename in additional_dirs.items():
        src_path = os.path.join(top_dir, dir_name, smart_stats_file)
        dst_path = os.path.join(os.getcwd(), new_filename)
        print(src_path)
        
        if os.path.exists(src_path):
            shutil.copy(src_path, dst_path)
            print(f"Copied and renamed {src_path} to {dst_path}")
        else:
            print(f"File not found: {src_path}")

def main():
    # Define paths to the files
    baseline_smart_stats_path = os.path.join(baseline_dir, smart_stats_file)
    starnuma_smart_stats_path = os.path.join(starnuma_dir, smart_stats_file)
    baseline_hop_stats_path = os.path.join(baseline_dir, hop_stats_file)
    starnuma_hop_stats_path = os.path.join(starnuma_dir, hop_stats_file)

    # Define output file names
    combined_smart_stats_output = os.path.join(os.getcwd(), "collected_smart_stats.txt")
    combined_hop_stats_output = os.path.join(os.getcwd(), "collected_hop_stats.txt")

    # Combine smart stats files
    combine_files(baseline_smart_stats_path, starnuma_smart_stats_path, combined_smart_stats_output)

    # Combine hop stats files
    combine_files(baseline_hop_stats_path, starnuma_hop_stats_path, combined_hop_stats_output)

    # Copy and rename additional smart stats files
    copy_and_rename_files()

if __name__ == "__main__":
    main()
