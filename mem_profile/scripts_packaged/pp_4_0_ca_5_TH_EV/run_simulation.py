
import subprocess

def run_simulation():
    # Use the full directory path for the executables and the output file
    subprocess.run("/nethome/acho44/StarNUMA_Artifact/mem_profile/scripts_packaged/pp_4_0_ca_5_TH_EV/omp_pp_trace_pp_4_0_ca_5_TH_EV | tee ppout_pp_4_0_ca_5_TH_EV.txt", shell=True, check=True)
    subprocess.run("/nethome/acho44/StarNUMA_Artifact/mem_profile/scripts_packaged/pp_4_0_ca_5_TH_EV/regenerate_page_maps_pp_4_0_ca_5_TH_EV", shell=True, check=True)

if __name__ == "__main__":
    run_simulation()
