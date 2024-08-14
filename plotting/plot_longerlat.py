from scipy.stats import gmean
import csv
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Patch


base_ipcs=[]
# Open the file and read the data
with open('collected_smart_stats.txt', 'r') as csvfile:
#with open('collected_smart_stats.csv', 'r') as csvfile:
    reader = csv.reader(csvfile)
    next(reader)  # Skip the header
    benchmarks = []
    normalized_IPCs = []
    base_amats = []
    cxi_amats = []
    for row in reader:
        #print(row)
        #if len(row) == 5:  # Make sure we have a full row of data
        benchmarks.append(row[0])
        base_IPC = float(row[1])
        base_ipcs.append(base_IPC)
        cxi_IPC = float(row[2])
        normalized_IPCs.append(cxi_IPC / base_IPC if base_IPC else 0)  # Avoid division by zero
        base_amats.append(float(row[3]))
        cxi_amats.append(float(row[4]))

with open('longerlat_collected_smart_stats.txt', 'r') as csvfile:
#with open('longerlat_collected_smart_stats.csv', 'r') as csvfile:
#with open('collected_smart_stats.csv', 'r') as csvfile:
    reader = csv.reader(csvfile)
    next(reader)  # Skip the header
    benchmarks_longerlat = []
    longerlat_normalized_IPCs = []
    i=0
    for row in reader:
        #print(row)
        #if len(row) == 5:  # Make sure we have a full row of data
        benchmarks_longerlat.append(row[0])
        longerlat_cxi_IPC = float(row[2])
        longerlat_normalized_IPCs.append(longerlat_cxi_IPC / base_ipcs[i] if base_ipcs[i] else 0)  # Avoid division by zero
        i=i+1
        #cxi_amats.append(float(row[4]))


# Calculate the geometric mean for normalized IPCs
geomean_normalized_IPCs = gmean(normalized_IPCs)
geomean_longerlat_normalized_IPCs = gmean(longerlat_normalized_IPCs)
# Calculate the arithmetic mean for AMATs
arithmetic_mean_base_amats = np.mean(base_amats)
arithmetic_mean_cxi_amats = np.mean(cxi_amats)

#speedup_diff = [normalized_IPCs[i]- longerlat_normalized_IPCs[i] for i in range(len(normalized_IPCs))]
#for i in range(len(normalized_IPCs)):
#    print(benchmarks[i]+' '+str(speedup_diff[i]))
#
#
for i in range(len(normalized_IPCs)):
    print(benchmarks[i]+' '+str(longerlat_normalized_IPCs[i]))
    print(benchmarks[i]+' '+str(normalized_IPCs[i]))



# adding an empty slot before mean
benchmarks.append('')
normalized_IPCs.append(0)
longerlat_normalized_IPCs.append(0)
base_amats.append(0)
cxi_amats.append(0)
benchmarks.append('MEAN')
normalized_IPCs.append(geomean_normalized_IPCs)
longerlat_normalized_IPCs.append(geomean_longerlat_normalized_IPCs)
print('Maineval  speedup mean: '+str(geomean_normalized_IPCs))
print('longerlat speedup mean: '+str(geomean_longerlat_normalized_IPCs))

plt.rcParams.update({'font.size': 14})
labelfontsize=20

#print(base_amats)

index = np.arange(len(benchmarks))
# Plot normalized IPCs
plt.figure(figsize=(10, 3))
barwidth=0.3
plt.bar(index-(barwidth*0.5), normalized_IPCs, width=barwidth, label='100ns CXL overhead', color='#9370db', edgecolor='black')
plt.bar(index+(barwidth*0.5), longerlat_normalized_IPCs, label='190ns CXL overhead' , width=barwidth, color='#DDA0DD', edgecolor='black')
plt.axhline(1, color='black', linestyle='--')
plt.grid(axis='y', linestyle='--')
plt.gca().set_axisbelow(True)  # Set grid behind
#plt.xlabel('Benchmark')
plt.ylabel('Normalized IPC', fontsize=labelfontsize)
plt.xticks(index, benchmarks)
plt.legend(ncol=1)
# Add annotation for geometric mean
mean_x_position = benchmarks.index('MEAN')
#plt.text(mean_x_position, geomean_normalized_IPCs + 0.02, f'{geomean_normalized_IPCs:.2f}', ha='center')
#plt.title('Normalized IPC for each benchmark')
#plt.xticks(rotation=90)
#plt.show()
plt.savefig('longerlat_IPC.png', bbox_inches='tight')
plt.savefig('longerlat_IPC.pdf', bbox_inches='tight')


