import os
import subprocess


import os

# Add dramsim path to LD_LIBRARY_PATH
starnuma_artifact_path = os.environ.get('STARNUMA_ARTIFACT_PATH')
if starnuma_artifact_path:
    current_ld_library_path = os.environ.get('LD_LIBRARY_PATH', '')
    new_path = f'{starnuma_artifact_path}/DRAMsim3/'
    os.environ['LD_LIBRARY_PATH'] = new_path + ':' + current_ld_library_path
    print(os.environ['LD_LIBRARY_PATH'])
else:
    print("STARNUMA_ARTIFACT_PATH is not set.")


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

