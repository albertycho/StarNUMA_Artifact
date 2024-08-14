import os
import pandas as pd

# Define paths
# Get the top directory from the environment variable
top_dir = os.getenv('STARNUMA_RUNS_TOP_DIR')

if top_dir is None:
    raise EnvironmentError("Environment variable STARNUMA_RUNS_TOP_DIR is not set.")


baseline_dir = os.path.join(top_dir, "baseline_main")
starnuma_dir = os.path.join(top_dir, "StarNUMA_270ns")

# Define file names
smart_stats_file = "collected_smart_stats.txt"
hop_stats_file = "collected_hop_stats.txt"

def combine_files(baseline_file, starnuma_file, output_file):
    # Load baseline and StarNUMA data
    baseline_df = pd.read_csv(baseline_file)
    starnuma_df = pd.read_csv(starnuma_file)
    
    # Initialize a new DataFrame for the combined data
    combined_df = baseline_df.copy()

    # Iterate over columns and replace 'cxi_' columns in the combined dataframe with values from starnuma_df
    for col in combined_df.columns:
        #print(col)
        if 'cxi_' in col:
            #print(col)
            combined_df[col] = starnuma_df[col]
    
    # Save the combined dataframe
    combined_df.to_csv(output_file, index=False)
    print(f"Combined file saved as: {output_file}")

def main():
    # Define paths to the files
    baseline_smart_stats_path = os.path.join(baseline_dir, smart_stats_file)
    starnuma_smart_stats_path = os.path.join(starnuma_dir, smart_stats_file)
    print(starnuma_smart_stats_path)
    baseline_hop_stats_path = os.path.join(baseline_dir, hop_stats_file)
    starnuma_hop_stats_path = os.path.join(starnuma_dir, hop_stats_file)

    # Define output file names
    combined_smart_stats_output = os.path.join(os.getcwd(), "collected_smart_stats.txt")
    combined_hop_stats_output = os.path.join(os.getcwd(), "collected_hop_stats.txt")

    # Combine smart stats files
    combine_files(baseline_smart_stats_path, starnuma_smart_stats_path, combined_smart_stats_output)

    # Combine hop stats files
    combine_files(baseline_hop_stats_path, starnuma_hop_stats_path, combined_hop_stats_output)

if __name__ == "__main__":
    main()
