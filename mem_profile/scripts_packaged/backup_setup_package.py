import argparse
import os
import shutil

import subprocess
import os

def parse_arguments():
    parser = argparse.ArgumentParser(description="Script to setup and prepare PP simulations.")
    
    parser.add_argument('--ps', type=int, default=4, help='Page size (default: 4)')
    parser.add_argument('--ml', type=int, default=32, help='Migration limit (default: 32)')
    parser.add_argument('--sh', type=int, default=0, help='Sharer (default: 0)')
    parser.add_argument('--pc', type=int, default=5, help='Pool capacity (default: 5)')
    
    args = parser.parse_args()

    assert args.ps in [4, 512], "ps must be either 4 or 512"
    assert args.ml in [0, 32, 256], "ml must be either 32 or 256"
    assert args.sh in [0, 1], "sh must be 0 or 1"
    assert args.pc in [5, 17], "pc must be 5 or 17"

    return args

def create_directory(ps, ml, sh, pc):
    sh_str = "ca" if sh == 0 else "sh"
    #dir_name = f"pp_{ps}_{ml}_{sh_str}_{pc}_TH"
    dir_name = f"pp_{ps}_{ml}_{sh_str}_{pc}_TH_EV"
    os.makedirs(dir_name, exist_ok=True)
    return dir_name

def copy_cpp_file(ps, sh, directory):
    base_dir = "BASE_FILES"
    cpp_file = "4KBpage_pp.cpp" if ps == 4 else "512KBpage_pp_TH_eviction.cpp"
    if ps==512 and sh==1:
        cpp_file = "512KBpage_pp.cpp"
    destfilename = "omp_pp_trace.cpp"
    src = os.path.join(base_dir, cpp_file)
    dest = os.path.join(directory, destfilename)
    shutil.copy(src, dest)

def modify_and_copy_regenerate_file(directory, ml,sh,pc):
    base_dir = "BASE_FILES"
    file_name = "regenerate_page_maps.cpp"
    src = os.path.join(base_dir, file_name)
    dest = os.path.join(directory, file_name)

    shca="CA"
    if(sh==1):
        shca="SH"
    
    
    input_dirname = "\"PP_"+str(ps)+"KB_"+shca+"_"+str(int(ml/1024))+"Kml_"+str(pc)+ "_TH_Phase_evict\""
    output_dirname = "\"pagemaps_"+str(ps)+"KB_"+shca+"_"+str(int(ml/1024))+"Kml_"+str(pc)+"_TH_evict/\""

    with open(src, 'r') as src_file:
        lines = src_file.readlines()

    with open(dest, 'w') as dest_file:
        for line in lines:
            if "#define MIGRATION_LIMIT" in line and not line.strip().startswith("//"):
                dest_file.write(f"#define MIGRATION_LIMIT {ml}\n")
            elif '#define INPUT_BASE_NAME' in line and not line.strip().startswith("//"):
                dest_file.write(f"#define INPUT_BASE_NAME {input_dirname}\n")
            elif '#define OUTPUT_BASE_NAME' in line and not line.strip().startswith("//"):
                dest_file.write(f"#define OUTPUT_BASE_NAME {output_dirname}\n")
            else:
                dest_file.write(line)

def modify_and_copy_hpp_file(directory,ps, ml, sh, pc):
    base_dir = "BASE_FILES"
    hpp_file = "4KBpage_pp.hpp" if ps == 4 else "512KBpage_pp.hpp"
    destfilename = "omp_pp_trace.hpp"
    src = os.path.join(base_dir, hpp_file)
    dest = os.path.join(directory, destfilename)

    ml2=ml
    if(ps==512):
        ml2=int(ml/128)

    sp=1
    sa=1
    sbs=0
    shca="CA"
    if(sh==1):
        sp=500000000
        sa=0
        sbs=1
        shca="SH"
    
    
    dirname = "\"PP_"+str(ps)+"KB_"+shca+"_"+str(int(ml/1024))+"Kml_"+str(pc)+ "_TH_Phase_evict\""

    with open(src, 'r') as src_file:
        lines = src_file.readlines()

    with open(dest, 'w') as dest_file:
        for line in lines:
            if "#define MIGRATION_LIMIT" in line and not line.strip().startswith("//"):
                dest_file.write(f"#define MIGRATION_LIMIT {ml2}\n")
                print(f"#define MIGRATION_LIMIT {ml2}\n")
            elif '#define POOL_FRACTION' in line and not line.strip().startswith("//"):
                dest_file.write(f"#define POOL_FRACTION {pc}\n")
            elif '#define SAMPLING_PERIOD' in line and not line.strip().startswith("//"):
                dest_file.write(f"#define SAMPLING_PERIOD {sp}\n")
            elif '#define SAMPLE_ALL' in line and not line.strip().startswith("//"):
                dest_file.write(f"#define SAMPLE_ALL {sa}\n")
            elif '#define SORT_BY_SHARERS' in line and not line.strip().startswith("//"):
                dest_file.write(f"#define SORT_BY_SHARERS {sbs}\n")
            elif '#define DIRNAME' in line and not line.strip().startswith("//"):
                dest_file.write(f"#define DIRNAME {dirname}\n")
            else:
                dest_file.write(line)




def compile_files(new_dir):
    compile_cmds = [
        f"g++ -Wall {new_dir}/omp_pp_trace.cpp -O3 -fopenmp -o {new_dir}/omp_pp_trace_{new_dir}",
        f"g++ {new_dir}/regenerate_page_maps.cpp -O3 -Wall -std=c++17 -o {new_dir}/regenerate_page_maps_{new_dir}"
    ]
    
    for cmd in compile_cmds:
        print(cmd)
        subprocess.run(cmd, shell=True, check=True)
    print("Compilation completed.")


def generate_executor_py(new_dir):
    # Get the full absolute path of the new directory
    full_dir_path = os.path.abspath(new_dir)
    
    executor_script_path = os.path.join(full_dir_path, "run_simulation.py")
    with open(executor_script_path, 'w') as f:
        f.write(f"""
import subprocess

def run_simulation():
    # Use the full directory path for the executables and the output file
    subprocess.run("{full_dir_path}/omp_pp_trace_{new_dir} | tee ppout_{new_dir}.txt", shell=True, check=True)
    subprocess.run("{full_dir_path}/regenerate_page_maps_{new_dir}", shell=True, check=True)

if __name__ == "__main__":
    run_simulation()
""")
    print(f"Executor Python script generated at {executor_script_path}")

# def generate_executor_py(new_dir):
#     executor_script_path = os.path.join(new_dir, "run_simulation.py")
#     with open(executor_script_path, 'w') as f:
#         f.write(f"""
# import subprocess

# def run_simulation():
#     subprocess.run("./omp_pp_trace_{new_dir} | tee ppout_{new_dir}.txt", shell=True, check=True)
#     subprocess.run("./regenerate_page_maps_{new_dir}", shell=True, check=True)

# if __name__ == "__main__":
#     run_simulation()
# """)
#     print(f"Executor Python script generated at {executor_script_path}")


# def copy_hpp_file(ps, directory):
#     base_dir = "BASE_FILES"
#     hpp_file = "4KBpage_pp.hpp" if ps == 4 else "512KBpage_pp.hpp"
#     src = os.path.join(base_dir, hpp_file)
#     dest = os.path.join(directory, hpp_file)
#     shutil.copy(src, dest)

if __name__ == "__main__":
    args = parse_arguments()
    
    ps = args.ps
    ml = args.ml
    sh = args.sh
    pc = args.pc
    
    new_dir = create_directory(ps, ml, sh, pc)
    print(f"Created new directory: {new_dir}")
    
    ml=ml*1024

    copy_cpp_file(ps, sh, new_dir)
    modify_and_copy_regenerate_file(new_dir, ml,sh,pc)
    modify_and_copy_hpp_file(new_dir,ps,ml, sh, pc)
    print("Files copied and modified as required.")


    compile_files(new_dir)
    generate_executor_py(new_dir)
    print("All tasks completed.")
