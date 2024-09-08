import argparse
import shutil
import os
import sys
import csv

starnuma_artifact_path = os.getenv('STARNUMA_ARTIFACT_PATH')

directories = ["BFS", "CC", "SSSP", "FMI", "MASST", "TPCC","POA"]

BFS_TRACE_PATH = starnuma_artifact_path+"/tracer/TRACES/BFS/"
BFS_SAMPLE_COUNT = 1
BFS_START_PHASE = 1

CC_TRACE_PATH = starnuma_artifact_path+"/tracer/TRACES/CC/"
CC_SAMPLE_COUNT = 1
CC_START_PHASE = 1

SSSP_TRACE_PATH = starnuma_artifact_path+"/tracer/TRACES/SSSP/"
SSSP_SAMPLE_COUNT = 1
SSSP_START_PHASE = 1

FMI_TRACE_PATH = starnuma_artifact_path+"/tracer/TRACES/FMI/"
FMI_SAMPLE_COUNT = 1
FMI_START_PHASE = 1

MASST_TRACE_PATH= starnuma_artifact_path+"/tracer/TRACES/MASST/"
MASST_SAMPLE_COUNT = 1
MASST_START_PHASE = 1

TPCC_TRACE_PATH =  starnuma_artifact_path+"/tracer/TRACES/TPCC/"
TPCC_SAMPLE_COUNT = 1
TPCC_START_PHASE = 1

POA_TRACE_PATH =  starnuma_artifact_path+"/tracer/TRACES/POA/"
POA_SAMPLE_COUNT = 1
POA_START_PHASE = 1

# Path to the n_phases.csv file
n_phases_csv_path = os.path.join(starnuma_artifact_path, 'tracer', 'TRACES', 'n_phases.csv')

# Dictionary to store the sample counts for each benchmark
sample_counts = {}

# Read the n_phases.csv file and populate the sample_counts dictionary
with open(n_phases_csv_path, mode='r') as file:
    csv_reader = csv.reader(file)
    for row in csv_reader:
        if len(row) == 2:
            benchmark, sample_count = row
            sample_counts[benchmark] = int(sample_count)

# Now, replace the *_SAMPLE_COUNT values based on the n_phases.csv file
BFS_SAMPLE_COUNT = sample_counts.get('BFS', BFS_SAMPLE_COUNT)
CC_SAMPLE_COUNT = sample_counts.get('CC', CC_SAMPLE_COUNT)
SSSP_SAMPLE_COUNT = sample_counts.get('SSSP', SSSP_SAMPLE_COUNT)
FMI_SAMPLE_COUNT = sample_counts.get('FMI', FMI_SAMPLE_COUNT)
MASST_SAMPLE_COUNT = sample_counts.get('MASSTREE', MASST_SAMPLE_COUNT)  # Ensure correct naming in n_phases.csv
TPCC_SAMPLE_COUNT = sample_counts.get('TPCC', TPCC_SAMPLE_COUNT)
POA_SAMPLE_COUNT = sample_counts.get('POA', POA_SAMPLE_COUNT)


champsim_path = starnuma_artifact_path+'/NUMA_Csim'
top_dir_path = starnuma_artifact_path+'/EX_DIR_HIER/'
base_run_smart_py_path =  starnuma_artifact_path+'/timingsim_runscripts/run_smarts.py'
base_run_smart_static_alloc_py_path= starnuma_artifact_path+ '/timingsim_runscripts/run_smarts_static_alloc.py'

def process_directories():
    # Define a dictionary to simulate a case statement with multiple variables
    dir_cases = {
        # Add additional cases here
        'StarNUMA_main': {
            'tmp_sn': True,
            'tmp_binname': "4C_16S",
            'tmp_pagemap_dir': "pagemaps_512KB_CA_256Kml_5"
        },
        'StarNUMA_270ns': {
            'tmp_sn': True,
            'tmp_binname': "4C_16S_270ns",
            'tmp_pagemap_dir': "pagemaps_512KB_CA_256Kml_5"
        },
        'StarNUMA_halfBW': {
            'tmp_sn': True,
            'tmp_binname': "4C_16S_SNUMA_halfBW",
            'tmp_pagemap_dir': "pagemaps_512KB_CA_256Kml_5"
        },
        'StarNUMA_smallpool': {
            'tmp_sn': True,
            'tmp_binname': "4C_16S",
            'tmp_pagemap_dir': "pagemaps_512KB_CA_256Kml_17"
        },
        'baseline_main': {
            'tmp_sn': False,
            'tmp_binname': "4C_16S",
            'tmp_pagemap_dir': "pagemaps_4KB_CA_0Kml_5"
        },
        'baseline_2XBW': {
            'tmp_sn': False,
            'tmp_binname': "4C_16S_baseline_2XBW",
            'tmp_pagemap_dir': "pagemaps_4KB_CA_0Kml_5/"
        },
        'baseline_ISOBW': {
            'tmp_sn': False,
            'tmp_binname': "4C_16S_baseline_ISOBW",
            'tmp_pagemap_dir': "pagemaps_4KB_CA_0Kml_5/"
        },

    }



    for dir_name in os.listdir(top_dir_path):
        if os.path.isdir(os.path.join(top_dir_path, dir_name)):
            case = dir_cases.get(dir_name)
            if case is not None:
                tmp_sn = case['tmp_sn']
                tmp_binname = case['tmp_binname']
                tmp_pagemap_dir = case['tmp_pagemap_dir']

                print(f"Processing directory: {dir_name}\ntmp_sn: {tmp_sn}\ntmp_binname: {tmp_binname}\ntmp_pagemap_dir: {tmp_pagemap_dir}")

                # Change to the directory
                os.chdir(os.path.join(top_dir_path, dir_name))

                for item in directories:
                    # Dynamically set the appropriate variables
                    tmp_SAMPLE_COUNT = globals()[f"{item}_SAMPLE_COUNT"]
                    tmp_START_PHASE = globals()[f"{item}_START_PHASE"]
                    tmp_TRACE_PATH = globals()[f"{item}_TRACE_PATH"]

                    # Print the values
                    print(f"tmp_SAMPLE_COUNT: {tmp_SAMPLE_COUNT}\ntmp_START_PHASE: {tmp_START_PHASE}\ntmp_TRACE_PATH: {tmp_TRACE_PATH}")


                    # Check if directory exists, if not, create it
                    if not os.path.exists(item):
                        os.makedirs(item)
                        print("created "+item+"dir")

                    # Change to the directory
                    os.chdir(item)

                    # Perform operations in the directory
                    print(f"Inside {item} directory. Using {tmp_SAMPLE_COUNT}, {tmp_START_PHASE}, {tmp_TRACE_PATH}.")


                    # Copy the base run_smarts.py file to the current directory
                    shutil.copy(base_run_smart_py_path, os.path.join(os.getcwd(), "run_smarts.py"))

                    # Read the contents of the copied file
                    with open("run_smarts.py", "r") as file:
                        filedata = file.readlines()

                    # Function to replace the first occurrence of a variable assignment
                    def replace_first_occurrence(lines, variable_name, replacement):
                        for i, line in enumerate(lines):
                            if variable_name in line:
                                lines[i] = replacement + "\n"
                                break
                        return lines

                    # Replace the first occurrences of the variables
                    filedata = replace_first_occurrence(filedata, 'sample_count', f"sample_count={tmp_SAMPLE_COUNT}")
                    filedata = replace_first_occurrence(filedata, 'default_first_phase', f"default_first_phase={tmp_START_PHASE}")
                    filedata = replace_first_occurrence(filedata, 'tr_path', f"tr_path='{tmp_TRACE_PATH}'")
                    filedata = replace_first_occurrence(filedata, 'pagemap_dir', f'pagemap_dir="{tmp_pagemap_dir}"')
                    filedata = replace_first_occurrence(filedata, 'champsim_path', f"champsim_path = '{champsim_path}'")
                    filedata = replace_first_occurrence(filedata, 'binname', f"binname='{tmp_binname}'")
                    filedata = replace_first_occurrence(filedata, 'sn', f"sn = {tmp_sn}")

                    # Write the modified content back to the file
                    with open("run_smarts.py", "w") as file:
                        file.writelines(filedata)

                    print("Modified run_smarts.py for", item)


                    # Change back to the parent directory (dir_name)
                    os.chdir('..')

                # Change back to the top directory
                os.chdir(top_dir_path)
            else:
                print(f"Directory: {dir_name} is not in the case statement.")



def process_static_alloc_directory():
    static_alloc_dir = os.path.join(top_dir_path, "STATIC_ALLOC")
    if os.path.exists(static_alloc_dir) and os.path.isdir(static_alloc_dir):
        os.chdir(static_alloc_dir)
        print(f"Processing STATIC_ALLOC directory: {static_alloc_dir}")

        for item in directories:
            tmp_SAMPLE_COUNT = globals()[f"{item}_SAMPLE_COUNT"]
            tmp_START_PHASE = globals()[f"{item}_START_PHASE"]
            tmp_TRACE_PATH = globals()[f"{item}_TRACE_PATH"]

            if not os.path.exists(item):
                os.makedirs(item)
                print(f"Created {item} directory inside STATIC_ALLOC.")

            os.chdir(item)

            shutil.copy(base_run_smart_static_alloc_py_path, os.path.join(os.getcwd(), "run_smarts_static_alloc.py"))

            with open("run_smarts_static_alloc.py", "r") as file:
                filedata = file.readlines()

            def replace_first_occurrence(lines, variable_name, replacement):
                for i, line in enumerate(lines):
                    if variable_name in line:
                        lines[i] = replacement + "\n"
                        break
                return lines

            filedata = replace_first_occurrence(filedata, 'sample_count', f"sample_count={tmp_SAMPLE_COUNT}")
            filedata = replace_first_occurrence(filedata, 'default_first_phase', f"default_first_phase={tmp_START_PHASE}")
            filedata = replace_first_occurrence(filedata, 'tr_path', f"tr_path='{tmp_TRACE_PATH}'")
            filedata = replace_first_occurrence(filedata, 'champsim_path', f"champsim_path = '{champsim_path}'")

            with open("run_smarts_static_alloc.py", "w") as file:
                file.writelines(filedata)

            print(f"Modified run_smarts_static_alloc.py for {item} inside STATIC_ALLOC.")

            os.chdir('..')



if __name__ == "__main__":
    process_directories()
    process_static_alloc_directory()
