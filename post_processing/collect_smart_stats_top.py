import os

outfile_smart_stats = open('collected_smart_stats.txt', 'w')
outfile_hop_stats = open('collected_hop_stats.txt', 'w')
outfile_mem_cacheblock_ratio = open('collected_mem_blocktransfer_stats.txt', 'w')

outfile_smart_stats.write("benchmark, base_IPC, cxi_IPC, base_amat, cxi_amat,\n")
#outfile_hop_stats.write("benchmark, base_S0_n_local, base_S0_n_1hop, base_S0_n_2hop, base_S0_n_CXO, cxi_S0_n_local, cxi_S0_n_1hop, cxi_S0_n_2hop, cxi_S0_n_CXO,\n")
outfile_hop_stats.write("benchmark, base_S0_n_local, base_S0_n_1hop, base_S0_n_2hop, base_S0_n_CXO, base_S0_bt_nopool, base_S0_bt_pool, cxi_S0_n_local, cxi_S0_n_1hop, cxi_S0_n_2hop, cxi_S0_n_CXO, cxi_S0_bt_nopool, cxi_S0_bt_pool,\n")
outfile_mem_cacheblock_ratio.write("benchmark, base_allacc, base_bt, base_cxl_accs, base_cxl_bt, cxi_allacc, cxi_bt, cxi_cxl_accs, cxi_cxl_bt,\n")

apps=['SSSP','BFS','CC','TC','MASST','TPCC'  ,'FMI','POA']
#apps=['BFS','CC','SSSP','TC','FMI','POA']
#apps=['BFS','CC','SSSP','TC','FMI','POA','dbg','pileup','chain','KMEANS_nopf_p2','MASSTREE']
#apps=['fixedwh_8k']
#for subdir, dirs, files in apps:
#for subdir, dirs, files in os.walk("."):
#    if 'smart_stat.txt' in files:

#subdirs = [subdir for subdir, _, _ in os.walk(".") if 'smart_stat.txt' in os.listdir(subdir)]
#for subdir in sorted(subdirs):
#        benchmark = subdir.split('/')[-1]

for app in apps:
    subdir = os.path.join('.', app)
    if os.path.exists(subdir) and 'smart_stat.txt' in os.listdir(subdir):
        benchmark = app
        base_IPC, base_amat, cxi_IPC, cxi_amat = None, None, None, None
        base_S0_n_2hop, base_S0_n_1hop, base_S0_n_local, base_S0_n_CXO = None, None, None, None
        base_S0_allaccs, base_S0_block_transfers, base_S0_CXL_block_transfers = None, None, None
        cxi_S0_n_2hop, cxi_S0_n_1hop, cxi_S0_n_local, cxi_S0_n_CXO = None, None, None, None
        cxi_S0_allaccs, cxi_S0_block_transfers, cxi_S0_CXL_block_transfers = None, None, None


        with open(os.path.join(subdir, 'smart_stat.txt'), 'r') as f:
            for line in f:
                if 'base_IPC' in line:
                    base_IPC = line.split(':')[1].strip()
                elif 'base_amat' in line:
                    base_amat = line.split(':')[1].strip()
                elif 'cxi_IPC' in line:
                    cxi_IPC = line.split(':')[1].strip()
                elif 'cxi_amat' in line:
                    cxi_amat = line.split(':')[1].strip()
                elif 'base_S0_n_2hop' in line:
                    base_S0_n_2hop = line.split(':')[1].strip()
                elif 'base_S0_n_1hop' in line:
                    base_S0_n_1hop = line.split(':')[1].strip()
                elif 'base_S0_n_local' in line:
                    base_S0_n_local = line.split(':')[1].strip()
                elif 'base_S0_n_CXO' in line:
                    base_S0_n_CXO = line.split(':')[1].strip()
                elif 'base_icn_s0allaccs' in line:
                    base_S0_allaccs = line.split(':')[1].strip()
                elif 'base_icn_s0_block_transfers' in line:
                    base_S0_block_transfers = line.split(':')[1].strip()
                elif 'base_icn_s0_CXL_block_transfers' in line:
                    base_S0_CXL_block_transfers = line.split(':')[1].strip() 
                elif 'cxi_S0_n_2hop' in line:
                    cxi_S0_n_2hop = line.split(':')[1].strip()
                elif 'cxi_S0_n_1hop' in line:
                    cxi_S0_n_1hop = line.split(':')[1].strip()
                elif 'cxi_S0_n_local' in line:
                    cxi_S0_n_local = line.split(':')[1].strip()
                elif 'cxi_S0_n_CXO' in line:
                    cxi_S0_n_CXO = line.split(':')[1].strip()

                elif 'cxi_icn_s0allaccs' in line:
                    cxi_S0_allaccs = line.split(':')[1].strip()
                elif 'cxi_icn_s0_block_transfers' in line:
                    cxi_S0_block_transfers = line.split(':')[1].strip()
                elif 'cxi_icn_s0_CXL_block_transfers' in line:
                    cxi_S0_CXL_block_transfers = line.split(':')[1].strip() 


        outfile_smart_stats.write(f"{benchmark}, {base_IPC}, {cxi_IPC}, {base_amat}, {cxi_amat},\n")
        outfile_hop_stats.write(f"{benchmark}, {base_S0_n_local}, {base_S0_n_1hop}, {base_S0_n_2hop}, {base_S0_n_CXO}, {base_S0_block_transfers}, {base_S0_CXL_block_transfers}, {cxi_S0_n_local}, {cxi_S0_n_1hop}, {cxi_S0_n_2hop}, {cxi_S0_n_CXO}, {cxi_S0_block_transfers}, {cxi_S0_CXL_block_transfers},\n")
        outfile_mem_cacheblock_ratio.write(f"{benchmark}, {base_S0_allaccs}, {base_S0_block_transfers}, {base_S0_n_CXO}, {base_S0_CXL_block_transfers}, {cxi_S0_allaccs}, {cxi_S0_block_transfers}, {cxi_S0_n_CXO}, {cxi_S0_CXL_block_transfers},\n")

#outfile_hop_stats.write("benchmark, base_S0_n_local, base_S0_n_1hop, base_S0_n_2hop, base_S0_n_CXO, base_S0_bt_nopool, base_S0_bt_pool, cxi_S0_n_local, cxi_S0_n_1hop, cxi_S0_n_2hop, cxi_S0_n_CXO, cxi_S0_bt_nopool, cxi_S0_bt_pool,\n")

outfile_smart_stats.close()
outfile_hop_stats.close()
outfile_mem_cacheblock_ratio.close()

exit(0)


outfile = open('collected_smart_stats.txt', 'w')
outfile.write("benchmark, base_IPC, cxi_IPC, base_amat, cxi_amat,\n")



for subdir, dirs, files in os.walk("."):
    if 'smart_stat.txt' in files:
        with open(os.path.join(subdir, 'smart_stat.txt'), 'r') as f:
            lines = f.readlines()
            base_IPC = lines[0].split(": ")[1].strip()
            base_amat = lines[1].split(": ")[1].strip()
            cxi_IPC = lines[2].split(": ")[1].strip()
            cxi_amat = lines[3].split(": ")[1].strip()
            benchmark = os.path.basename(subdir)
            outfile.write(f"{benchmark}, {base_IPC}, {cxi_IPC}, {base_amat}, {cxi_amat},\n")

outfile.close()

