import os
import numpy as np
import re

def process_directories(startswith, all_insts, all_cycles, latency_sum, memaccs_sum, icn_S0_accs_sum, S0_block_transfers, S0_CXL_block_trasnfers, S0_n_values, ipcs, order, icnlat_noncxl_ma, icnlat_cxl_ma, icnlat_noncxl_bt, icnlat_cxl_bt, icnlat_all, icn_noncxl_ma, icn_cxl_ma, icn_noncxl_bt, icn_cxl_bt):
#def process_directories(startswith, all_insts, all_cycles, latency_sum, memaccs_sum, icn_S0_accs_sum, S0_block_transfers,S0_CXL_block_trasnfers, S0_n_values, ipcs, order):
    S0_n_values['S0_n_2hop']=0;
    S0_n_values['S0_n_1hop']=0;
    S0_n_values['S0_n_local']=0;
    S0_n_values['S0_n_CXO']=0;
    S0_n_values['S0_n_block_trasnfers']=0;
    S0_n_values['S0_n_homeCXL_but_block_transfers']=0;
    icn_allp=0;
    for subdir, dirs, files in os.walk("."):
        if subdir.startswith('./'+startswith):
            thisrun_insts=0;
            thisrun_cycles=0;
            memaccs=0;
            flag0=False;
            with open(os.path.join(subdir, 'res.txt'), 'r') as f:
                lines = f.readlines()
                region_of_interest_found = False
                for line in lines:
                    if "Region of Interest Statistics" in line:
                        print(subdir)
                        region_of_interest_found = True
                    elif region_of_interest_found:
                        if "cumulative IPC" in line:
                            #print(line)
                            instructions = int(line.split('instructions: ')[1].split()[0])
                            cycles = int(line.split('cycles: ')[1].split()[0])
                            all_insts += instructions
                            all_cycles += cycles
                            thisrun_insts +=instructions
                            thisrun_cycles += cycles
                        elif "LLC TOTAL     ACCESS:" in line:
                            miss_str = re.search('MISS:\s+(\d+)', line)
                            if miss_str is not None:
                                memaccs = int(miss_str.group(1))
                                memaccs_sum = memaccs+memaccs_sum
                                assert(flag0==False)
                                flag0=True
                        elif "LLC AVERAGE MISS LATENCY:" in line:
                            latency = float((line.split('LLC AVERAGE MISS LATENCY: ')[1].strip()).split(' ')[0])
                            latency = latency / 2.4  # translate to ns from cycle
                            assert(flag0)
                            latency_sum = latency_sum+latency*memaccs
                            flag0=False
                            memaccs=0
                        #elif "S0_n_2hop" in line or "S0_n_1hop" in line or "S0_n_local" in line or "S0_n_CXO" in line:
                        elif "S0_n_2hop" in line or "S0_n_1hop" in line or "S0_n_local" in line or "S0_n_CXO" in line or "S0_n_block_trasnfers" in line or "S0_n_homeCXL_but_block_transfers" in line:
                            value_name = line.split(":")[0].strip()
                            value = int(line.split(":")[1].strip())
                            S0_n_values[value_name] = value + S0_n_values[value_name]
                            icn_S0_accs_sum= icn_S0_accs_sum+ value;
                            if "S0_n_block_trasnfers" in line:
                                S0_block_transfers = S0_block_transfers+value;
                            if "S0_n_homeCXL_but_block_transfers" in line:
                                S0_CXL_block_trasnfers = S0_CXL_block_trasnfers+value;
                        elif "Average icn lat for cxl access" in line:
                            match = re.search(r'access: (\d+), \((\d+)\)', line)
                            if match:
                                lat = int(match.group(1))
                                num = int(match.group(2))
                            else:
                                lat = 0
                                num = 0
                            latsum = lat*num
                            icnlat_cxl_ma= icnlat_cxl_ma + latsum;
                            icn_cxl_ma = icn_cxl_ma + num

                        elif "Average icn lat NON-cxl access" in line:
                            match = re.search(r'access: (\d+), \((\d+)\)', line)
                            if match:
                                lat = int(match.group(1))
                                num = int(match.group(2))
                            else:
                                lat = 0
                                num = 0
                            latsum = lat*num
                            icnlat_noncxl_ma= icnlat_noncxl_ma + latsum;
                            icn_noncxl_ma = icn_noncxl_ma + num
                            print("lat: "+str(lat))
                            print("num: "+str(num))
                            print("latsum: "+str(latsum))
                        elif "Average icn lat for cxl blktrs" in line:
                            match = re.search(r'blktrs: (\d+), \((\d+)\)', line)
                            if match:
                                lat = int(match.group(1))
                                num = int(match.group(2))
                            else:
                                lat = 0
                                num = 0
                            latsum = lat*num
                            icnlat_cxl_bt= icnlat_cxl_bt + latsum;
                            icn_cxl_bt = icn_cxl_bt + num

                        elif "Average icn lat NON-cxl blktrs" in line:
                            match = re.search(r'blktrs: (\d+), \((\d+)\)', line)
                            if match:
                                lat = int(match.group(1))
                                num = int(match.group(2))
                            else:
                                lat = 0
                                num = 0
                            latsum = lat*num
                            icnlat_noncxl_bt= icnlat_noncxl_bt + latsum;
                            icn_noncxl_bt = icn_noncxl_bt + num

                        elif "Average icn lat for everything" in line:
                            match = re.search(r'everything: (\d+), \((\d+)\)', line)
                            if match:
                                lat = int(match.group(1))
                                num = int(match.group(2))
                            else:
                                lat = 0
                                num = 0
                            latsum = lat*num
                            icnlat_all= icnlat_all + latsum
                            icn_allp=icn_allp+num

            ipc=0
            if(thisrun_cycles>0):
                ipc=thisrun_insts / thisrun_cycles;
            ipcs.append(ipc)
            order.append(subdir)

    if(icn_noncxl_ma>0):
        print(icnlat_noncxl_ma)
        print(icn_noncxl_ma)
        icnlat_noncxl_ma = icnlat_noncxl_ma/icn_noncxl_ma
    if(icn_noncxl_bt>0):
        icnlat_noncxl_bt = icnlat_noncxl_bt/icn_noncxl_bt

    if(icn_cxl_ma>0):
        icnlat_cxl_ma = icnlat_cxl_ma/icn_cxl_ma

    if(icn_cxl_bt>0):
        icnlat_cxl_bt = icnlat_cxl_bt/icn_cxl_bt
    if(icn_allp>0):
        icnlat_all = icnlat_all / icn_allp

    S0_n_values['S0_n_block_trasnfers']= S0_n_values['S0_n_block_trasnfers'] - S0_n_values['S0_n_homeCXL_but_block_transfers'];
    S0_block_transfers = S0_block_transfers - S0_CXL_block_trasnfers

    #return all_insts, all_cycles, latency_sum, memaccs_sum, icn_S0_accs_sum, S0_block_transfers,S0_CXL_block_trasnfers, S0_n_values, ipcs, order
    return all_insts, all_cycles, latency_sum, memaccs_sum, icn_S0_accs_sum, S0_block_transfers, S0_CXL_block_trasnfers, S0_n_values, ipcs, order, icnlat_noncxl_ma, icnlat_cxl_ma, icnlat_noncxl_bt, icnlat_cxl_bt, icnlat_all, icn_noncxl_ma, icn_cxl_ma, icn_noncxl_bt, icn_cxl_bt

# For directories starting with "base_phase_"
base_all_insts, base_all_cycles, base_latency_sum, base_memaccs_sum, base_icn_S0_accs_sum, base_S0_block_transfers,base_S0_CXL_block_transfers , base_S0_n_values, base_ipcs, base_order = 0, 0, 0,0,0,0,0, {},[],[]
base_icnlat_noncxl_ma, base_icnlat_cxl_ma, base_icnlat_noncxl_bt, base_icnlat_cxl_bt, base_icnlat_all = 0, 0, 0, 0, 0
base_icn_noncxl_ma, base_icn_cxl_ma, base_icn_noncxl_bt, base_icn_cxl_bt = 0, 0, 0, 0

base_all_insts, base_all_cycles, base_latency_sum, base_memaccs_sum, base_icn_S0_accs_sum, base_S0_block_transfers, base_S0_CXL_block_transfers, base_S0_n_values, base_ipcs, base_order, base_icnlat_noncxl_ma, base_icnlat_cxl_ma, base_icnlat_noncxl_bt, base_icnlat_cxl_bt, base_icnlat_all, base_icn_noncxl_ma, base_icn_cxl_ma, base_icn_noncxl_bt, base_icn_cxl_bt = process_directories("base_phase_", base_all_insts, base_all_cycles, base_latency_sum, base_memaccs_sum, base_icn_S0_accs_sum, base_S0_block_transfers, base_S0_CXL_block_transfers, base_S0_n_values, base_ipcs, base_order, base_icnlat_noncxl_ma, base_icnlat_cxl_ma, base_icnlat_noncxl_bt, base_icnlat_cxl_bt, base_icnlat_all, base_icn_noncxl_ma, base_icn_cxl_ma, base_icn_noncxl_bt, base_icn_cxl_bt)


#base_all_insts, base_all_cycles, base_latency_sum, base_memaccs_sum, base_icn_S0_accs_sum, base_S0_block_transfers,base_S0_CXL_block_transfers , base_S0_n_values, base_ipcs, base_order = process_directories("base_phase_", base_all_insts, base_all_cycles, base_latency_sum, base_memaccs_sum, base_icn_S0_accs_sum, base_S0_block_transfers,base_S0_CXL_block_transfers , base_S0_n_values, base_ipcs, base_order)
base_IPC = base_all_insts / base_all_cycles if base_all_cycles != 0 else 0
base_amat = base_latency_sum / base_memaccs_sum if base_memaccs_sum else 0
base_MPKI = 1000*base_memaccs_sum / base_all_insts if base_all_insts !=0 else 0

# For directories starting with "cxi_phase_"
cxi_all_insts, cxi_all_cycles, cxi_latency_sum, cxi_memaccs_sum, cxi_icn_S0_accs_sum, cxi_S0_block_transfers,cxi_S0_CXL_block_transfers , cxi_S0_n_values, cxi_ipcs, cxi_order = 0, 0,0,0,0,0,0, {},[],[]
cxi_icnlat_noncxl_ma, cxi_icnlat_cxl_ma, cxi_icnlat_noncxl_bt, cxi_icnlat_cxl_bt, cxi_icnlat_all = 0, 0, 0, 0, 0
cxi_icn_noncxl_ma, cxi_icn_cxl_ma, cxi_icn_noncxl_bt, cxi_icn_cxl_bt = 0, 0, 0, 0

cxi_all_insts, cxi_all_cycles, cxi_latency_sum, cxi_memaccs_sum, cxi_icn_S0_accs_sum, cxi_S0_block_transfers, cxi_S0_CXL_block_transfers, cxi_S0_n_values, cxi_ipcs, cxi_order, cxi_icnlat_noncxl_ma, cxi_icnlat_cxl_ma, cxi_icnlat_noncxl_bt, cxi_icnlat_cxl_bt, cxi_icnlat_all, cxi_icn_noncxl_ma, cxi_icn_cxl_ma, cxi_icn_noncxl_bt, cxi_icn_cxl_bt = process_directories("cxi_phase_", cxi_all_insts, cxi_all_cycles, cxi_latency_sum, cxi_memaccs_sum, cxi_icn_S0_accs_sum, cxi_S0_block_transfers, cxi_S0_CXL_block_transfers, cxi_S0_n_values, cxi_ipcs, cxi_order, cxi_icnlat_noncxl_ma, cxi_icnlat_cxl_ma, cxi_icnlat_noncxl_bt, cxi_icnlat_cxl_bt, cxi_icnlat_all, cxi_icn_noncxl_ma, cxi_icn_cxl_ma, cxi_icn_noncxl_bt, cxi_icn_cxl_bt)

#cxi_all_insts, cxi_all_cycles, cxi_latency_sum, cxi_memaccs_sum, cxi_icn_S0_accs_sum, cxi_S0_block_transfers,cxi_S0_CXL_block_transfers, cxi_S0_n_values,cxi_ipcs,cxi_order = process_directories("cxi_phase_", cxi_all_insts, cxi_all_cycles, cxi_latency_sum, cxi_memaccs_sum, cxi_icn_S0_accs_sum, cxi_S0_block_transfers,cxi_S0_CXL_block_transfers, cxi_S0_n_values, cxi_ipcs, cxi_order)
cxi_IPC = cxi_all_insts / cxi_all_cycles if cxi_all_cycles != 0 else 0
cxi_amat = (cxi_latency_sum / cxi_memaccs_sum) if cxi_memaccs_sum else 0

cxi_MPKI = 1000*cxi_memaccs_sum / cxi_all_insts if cxi_all_insts !=0 else 0

# Write to a text file
with open('smart_stat.txt', 'w') as f:
    f.write(f'base_IPC: {base_IPC}\n')
    f.write(f'base_amat: {base_amat}\n')
    for value_name, value in base_S0_n_values.items():
        f.write(f'base_{value_name}: {value}\n')
    f.write(f'base_icn_s0allaccs: {base_icn_S0_accs_sum}\n')
    f.write(f'base_icn_s0_block_transfers: {base_S0_block_transfers}\n')
    f.write(f'base_icn_s0_CXL_block_transfers: {base_S0_CXL_block_transfers}\n')
    # Writing out for base_ variables
    

    f.write(f'base_MPKI: {base_MPKI}\n\n')
    
    f.write(f'cxi_IPC: {cxi_IPC}\n')
    f.write(f'cxi_amat: {cxi_amat}\n')
    for value_name, value in cxi_S0_n_values.items():
        f.write(f'cxi_{value_name}: {value}\n')
    f.write(f'cxi_icn_s0allaccs: {cxi_icn_S0_accs_sum}\n')
    f.write(f'cxi_icn_s0_block_transfers: {cxi_S0_block_transfers}\n')
    f.write(f'cxi_icn_s0_CXL_block_transfers: {cxi_S0_CXL_block_transfers}\n')


    f.write(f'cxi_MPKI: {cxi_MPKI}\n\n')

    #for rname in base_order:
    for i in range(len(base_order)):
        f.write(base_order[i]+': '+str(base_ipcs[i])+'\n')
    for i in range(len(cxi_order)):
        f.write(cxi_order[i]+': '+str(cxi_ipcs[i])+'\n')


def divide_and_convert(value):
    return int(value / 2.4)

with open('icn_latency_stats.txt', 'w') as ff:
    ff.write(f'base_icnlat_noncxl_ma: {divide_and_convert(base_icnlat_noncxl_ma)}\n')
    ff.write(f'base_icnlat_cxl_ma: {divide_and_convert(base_icnlat_cxl_ma)}\n')
    ff.write(f'base_icnlat_noncxl_bt: {divide_and_convert(base_icnlat_noncxl_bt)}\n')
    ff.write(f'base_icnlat_cxl_bt: {divide_and_convert(base_icnlat_cxl_bt)}\n')
    ff.write(f'base_icnlat_all: {divide_and_convert(base_icnlat_all)}\n')

    ff.write(f'base_icn_noncxl_ma: {base_icn_noncxl_ma}\n')
    ff.write(f'base_icn_cxl_ma: {base_icn_cxl_ma}\n')
    ff.write(f'base_icn_noncxl_bt: {base_icn_noncxl_bt}\n')
    ff.write(f'base_icn_cxl_bt: {base_icn_cxl_bt}\n\n')

    ff.write(f'cxi_icnlat_noncxl_ma: {divide_and_convert(cxi_icnlat_noncxl_ma)}\n')
    ff.write(f'cxi_icnlat_cxl_ma: {divide_and_convert(cxi_icnlat_cxl_ma)}\n')
    ff.write(f'cxi_icnlat_noncxl_bt: {divide_and_convert(cxi_icnlat_noncxl_bt)}\n')
    ff.write(f'cxi_icnlat_cxl_bt: {divide_and_convert(cxi_icnlat_cxl_bt)}\n')
    ff.write(f'cxi_icnlat_all: {divide_and_convert(cxi_icnlat_all)}\n')

    ff.write(f'cxi_icn_noncxl_ma: {cxi_icn_noncxl_ma}\n')
    ff.write(f'cxi_icn_cxl_ma: {cxi_icn_cxl_ma}\n')
    ff.write(f'cxi_icn_noncxl_bt: {cxi_icn_noncxl_bt}\n')
    ff.write(f'cxi_icn_cxl_bt: {cxi_icn_cxl_bt}\n')


# with open('icn_latency_stats.txt', 'w') as ff:
#     ff.write(f'base_icnlat_noncxl_ma: {base_icnlat_noncxl_ma}\n')
#     ff.write(f'base_icnlat_cxl_ma: {base_icnlat_cxl_ma}\n')
#     ff.write(f'base_icnlat_noncxl_bt: {base_icnlat_noncxl_bt}\n')
#     ff.write(f'base_icnlat_cxl_bt: {base_icnlat_cxl_bt}\n')
#     ff.write(f'base_icnlat_all: {base_icnlat_all}\n')

#     ff.write(f'base_icn_noncxl_ma: {base_icn_noncxl_ma}\n')
#     ff.write(f'base_icn_cxl_ma: {base_icn_cxl_ma}\n')
#     ff.write(f'base_icn_noncxl_bt: {base_icn_noncxl_bt}\n')
#     ff.write(f'base_icn_cxl_bt: {base_icn_cxl_bt}\n\n')

#     ff.write(f'cxi_icnlat_noncxl_ma: {cxi_icnlat_noncxl_ma}\n')
#     ff.write(f'cxi_icnlat_cxl_ma: {cxi_icnlat_cxl_ma}\n')
#     ff.write(f'cxi_icnlat_noncxl_bt: {cxi_icnlat_noncxl_bt}\n')
#     ff.write(f'cxi_icnlat_cxl_bt: {cxi_icnlat_cxl_bt}\n')
#     ff.write(f'cxi_icnlat_all: {cxi_icnlat_all}\n')
#     ff.write(f'cxi_icn_noncxl_ma: {cxi_icn_noncxl_ma}\n')
#     ff.write(f'cxi_icn_cxl_ma: {cxi_icn_cxl_ma}\n')
#     ff.write(f'cxi_icn_noncxl_bt: {cxi_icn_noncxl_bt}\n')
#     ff.write(f'cxi_icn_cxl_bt: {cxi_icn_cxl_bt}\n')






exit(0)
