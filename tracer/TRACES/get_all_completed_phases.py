import os
import glob
import csv

def find_largest_N_with_min_files(directory, prefix, min_files=64):
    N = 0
    while True:
        # Create a search pattern for the current N
        pattern = os.path.join(directory, f"{prefix}*_N.out".replace("N", str(N)))
        
        # Find all files matching the pattern
        files = glob.glob(pattern)
        
        # Check if there are enough files
        if len(files) >= min_files:
            N += 1
        else:
            break

    return N - 1 if N > 0 else None

# Define the directories to check
directories = ['BFS', 'CC', 'FMI', 'MASSTREE', 'POA', 'SSSP', 'TC', 'TPCC']
prefix = 'mtrace_t'
output_file = 'n_phases.csv'

# Prepare data for CSV
results = []

for directory in directories:
    # Check for largest N in each directory
    full_path = os.path.join(os.getcwd(), directory)
    largest_N = find_largest_N_with_min_files(full_path, prefix)
    
    # Store result as (directory, largest_N)
    results.append([directory, largest_N if largest_N is not None else 0])

# Write results to n_phases.csv
with open(output_file, mode='w', newline='') as file:
    writer = csv.writer(file)
    writer.writerows(results)

print(f"Results have been written to {output_file}")

