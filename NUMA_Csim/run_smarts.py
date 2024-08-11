import argparse
import shutil
import os
import sys

##Runcommand format: bin_path+" --warmup_instructions "+str(wi)+" --simulation_instructions "+str(int(si))+\
##            " --tb_cpu_trace_path "+trace_dir_path+" --tb_cpu_forward "+str(tb_forward_i)+" "+cs_trace_path_0 +cs_trace_path_1 +cs_trace_path_2 +cs_trace_path_3


NBILLION=1000000000
NMILLION=1000000

SAMPLE_COUNT=4
default_first_phase=3
tr_path='/scratch/acho44/NUMACXL/TRACES/BFS/64T_0909/'
pagemap_dir="/pagemaps_4KB_CA_256Kml/"
champsim_path = '/nethome/acho44/NUMA_Csim'
binname='4C_16S'

def champsim_single_run(bin_path, wi, si, trace_dir_path, phase, iscxl):
    #phase input here refers to 1B insturction phase
    phase_str = str(phase)
    if(phase<100):
        phase_str='0'+phase_str
    if(phase<10):
        phase_str='0'+phase_str
   
    
    dirname='base_phase_'+phase_str
    if(iscxl):
        dirname='cxi_phase_'+phase_str

    # Make sure directory is unique
    #dir_path = os.path.join(trace_dir_path, dirname)
    dir_path =  dirname
    i = 1
    while os.path.isdir(dir_path):
        #dir_path = f"{dirname}_{i}"
        dir_path = "{}_{}".format(dirname, i)
        i += 1

    os.makedirs(dir_path, exist_ok=True)
    os.chdir(dir_path)
    
    tb_forward_i=phase*NBILLION
    #TODO change when running 4 cores
    cs_trace_path_0 = trace_dir_path+"/champsim_0_"+str(phase)+".trace.xz "
    cs_trace_path_1 = trace_dir_path+"/champsim_1_"+str(phase)+".trace.xz "
    cs_trace_path_2 = trace_dir_path+"/champsim_2_"+str(phase)+".trace.xz "
    cs_trace_path_3 = trace_dir_path+"/champsim_3_"+str(phase)+".trace.xz "
    cs_cmd=bin_path+" --warmup_instructions "+str(wi)+" --simulation_instructions "+str(int(si))+\
            " --tb_cpu_trace_path "+trace_dir_path+" --tb_cpu_forward "+str(tb_forward_i)+" "+cs_trace_path_0 +cs_trace_path_1 +cs_trace_path_2 +cs_trace_path_3

    cs_cmd=cs_cmd+" 1> res.txt 2>err.txt &"
    with open("cs_runcmd.txt", mode="wt") as f:
        f.write(cs_cmd+'\n')
        f.close()
    

    po_dir_prefix=trace_dir_path+pagemap_dir
    po_dir_name = po_dir_prefix+"/phase"+str(phase)+"_pagemaps/"
    if(iscxl):
        po_dir_name = po_dir_prefix+"/phase"+str(phase)+"_pagemaps_CI/"


    po_pre_filename = po_dir_name+"/page_owner_pre.txt"
    po_post_filename = po_dir_name+"/page_owner_post.txt"

    shutil.copy2(po_pre_filename, "page_owner_pre.txt")
    shutil.copy2(po_post_filename, "page_owner_post.txt")

    os.system(cs_cmd)
    #print(cs_cmd)

    #print("bin_path: ", bin_path)
    #print("wi: ", int(wi))
    #print("si: ", int(si))
    #print("trace_dir_path: ", trace_dir_path)
    #print("phase: ", int(phase))
   
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Script to run Champsim')

    # Add arguments
    parser.add_argument('--trace_dir_path', type=str, default=tr_path, help='Path to the trace directory')
    parser.add_argument('--bin_name', type=str, default=binname, help='Name of the binary')
    parser.add_argument('--checkpoint_interval', type=int, default=1, help='Interval for checkpoints')
    parser.add_argument('--first_phase', type=int, default=default_first_phase, help='first sampling phase')
    parser.add_argument('--si', type=int, default=100, help='simulated instructions per checkpoints, in millions')

    args = parser.parse_args()

    bin_path = champsim_path+'/bin/'+args.bin_name
    print("Command Line Arguments:")                                               
    print("\tTrace Directory Path:", args.trace_dir_path)                             
    print("\tCheckpoint Interval:", args.checkpoint_interval)                         
    print("\tSim Ins per checkpoint:", args.si)                           
    print("\tBin Path:", bin_path)                                                  
                                        
    ### TODO do precheck with checkpoint interval and migration interval
    last_sample_i= (args.checkpoint_interval*SAMPLE_COUNT)+args.first_phase
    last_trace_path = args.trace_dir_path+"/champsim_0_"+str(last_sample_i)+".trace.xz"
    if not os.path.isfile(last_trace_path):
        #print(f"File {last_trace_path} does not exist")
        print("File" + last_trace_path+ " does not exist")
        sys.exit(1)


    wi=20000000
    si=args.si*1000000
    # For now, I'm just using dummy values for the parameters of champsim_single_run.

    initial_directory = os.getcwd()
    for i in range(SAMPLE_COUNT):  # replace ... with your actual range
        # change to initial directory
        os.chdir(initial_directory)
        runphase = int(args.first_phase + (i*args.checkpoint_interval))
        champsim_single_run(bin_path, wi, si, args.trace_dir_path, runphase, False)
        os.chdir(initial_directory)
        champsim_single_run(bin_path, wi, si, args.trace_dir_path, runphase, True)
