import os
import subprocess

# Directory containing the configuration files
configs_dir = 'CONFIGS'

# List all files in the directory
config_files = [f for f in os.listdir(configs_dir) if f.endswith('.json')]

# Loop through each file and execute the required commands
for cfg in config_files:
    cfg_path = os.path.join(configs_dir, cfg)
    
    # Call the config.py script with the cfgjson file
    subprocess.run(['python3', 'config.py', cfg_path], check=True)
    
    # Run the 'make -j16' command
    subprocess.run(['make', '-j16'], check=True)

print("All configurations have been processed.")

