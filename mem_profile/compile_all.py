import os
import subprocess

# Step 1: Change directory to scripts_packaged
os.chdir("scripts_packaged")

# Step 2: Run the setup_package.py script with different arguments
subprocess.run(["python3", "setup_package.py", "--ps", "4", "--ml", "0"], check=True)
subprocess.run(["python3", "setup_package.py", "--ps", "512", "--ml", "256"], check=True)
subprocess.run(["python3", "setup_package.py", "--ps", "512", "--ml", "256", "--pc", "17"], check=True)

# Step 3: Go back to the parent directory
os.chdir("..")

# Step 4: Change directory to static_placement
os.chdir("static_placement")

# Step 5: Compile the C++ file with g++
subprocess.run(["g++", "-Wall", "omp_pp_trace.cpp", "-O3", "-fopenmp", "-o", "static_placement"], check=True)

