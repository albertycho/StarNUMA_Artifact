import os
import glob

def find_largest_N_with_min_files(prefix, min_files=64):
    N = 0
    directory = os.getcwd()  # Set the directory to the current working directory
    
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

# Usage
prefix = 'mtrace_t'
largest_N = find_largest_N_with_min_files(prefix)

if largest_N is not None:
    print(f"The largest N with 64 or more files is: {largest_N}")
else:
    print("No such N found.")

