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


with open('b_2X_collected_smart_stats.txt', 'r') as csvfile:
#with open('collected_smart_stats.csv', 'r') as csvfile:
    reader = csv.reader(csvfile)
    next(reader)  # Skip the header
    benchmarks_b_2X = []
    b_2X_normalized_IPCs = []
    i=0
    for row in reader:
        #print(row)
        #if len(row) == 5:  # Make sure we have a full row of data
        benchmarks_b_2X.append(row[0])
        b_2X_base_IPC = float(row[1])
        b_2X_normalized_IPCs.append(b_2X_base_IPC / base_ipcs[i] if base_ipcs[i] else 0)  # Avoid division by zero
        i=i+1
        #cxi_amats.append(float(row[4]))

with open('b_ISOBW_collected_smart_stats.txt', 'r') as csvfile:
#with open('collected_smart_stats.csv', 'r') as csvfile:
    reader = csv.reader(csvfile)
    next(reader)  # Skip the header
    benchmarks_b_ISOBW = []
    b_ISOBW_normalized_IPCs = []
    i=0
    for row in reader:
        #print(row)
        #if len(row) == 5:  # Make sure we have a full row of data
        benchmarks_b_ISOBW.append(row[0])
        b_ISOBW_base_IPC = float(row[1])
        b_ISOBW_normalized_IPCs.append(b_ISOBW_base_IPC / base_ipcs[i] if base_ipcs[i] else 0)  # Avoid division by zero
        i=i+1

with open('SNUMA_halfBW_collected_smart_stats.txt', 'r') as csvfile:
#with open('collected_smart_stats.csv', 'r') as csvfile:
    reader = csv.reader(csvfile)
    next(reader)  # Skip the header
    benchmarks_SNUMA_halfBW = []
    SNUMA_halfBW_normalized_IPCs = []
    i=0
    for row in reader:
        #print(row)
        #if len(row) == 5:  # Make sure we have a full row of data
        benchmarks_SNUMA_halfBW.append(row[0])
        SNUMA_halfBW_cxi_IPC = float(row[2])
        SNUMA_halfBW_normalized_IPCs.append(SNUMA_halfBW_cxi_IPC / base_ipcs[i] if base_ipcs[i] else 0)  # Avoid division by zero
        i=i+1


print(SNUMA_halfBW_normalized_IPCs)
print(b_2X_normalized_IPCs)
# Calculate the geometric mean for normalized IPCs
geomean_normalized_IPCs = gmean(normalized_IPCs)

geomean_b_2X_normalized_IPCs = gmean(b_2X_normalized_IPCs)
geomean_b_ISOBW_normalized_IPCs = gmean(b_ISOBW_normalized_IPCs)
geomean_SNUMA_halfBW_normalized_IPCs = gmean(SNUMA_halfBW_normalized_IPCs)
# Calculate the arithmetic mean for AMATs
arithmetic_mean_base_amats = np.mean(base_amats)
arithmetic_mean_cxi_amats = np.mean(cxi_amats)

for i in range(len(normalized_IPCs)):
    print(benchmarks[i]+' '+str(normalized_IPCs[i]))
    print(benchmarks[i]+' '+str(b_2X_normalized_IPCs[i]))
    print(benchmarks[i]+' '+str(b_ISOBW_normalized_IPCs[i]))
    print(benchmarks[i]+' '+str(SNUMA_halfBW_normalized_IPCs[i]))






# adding an empty slot before mean
benchmarks.append('')
normalized_IPCs.append(0)
b_2X_normalized_IPCs.append(0)
b_ISOBW_normalized_IPCs.append(0)
SNUMA_halfBW_normalized_IPCs.append(0)
base_amats.append(0)
cxi_amats.append(0)
benchmarks.append('MEAN')
normalized_IPCs.append(geomean_normalized_IPCs)
b_2X_normalized_IPCs.append(geomean_b_2X_normalized_IPCs)
b_ISOBW_normalized_IPCs.append(geomean_b_ISOBW_normalized_IPCs)
SNUMA_halfBW_normalized_IPCs.append(geomean_SNUMA_halfBW_normalized_IPCs)
print('Maineval  speedup mean: '+str(geomean_normalized_IPCs))
print('b_2X speedup mean: '+str(geomean_b_2X_normalized_IPCs))
print('b_ISOBW speedup mean: '+str(geomean_b_ISOBW_normalized_IPCs))
print('SNUMA_halfBW speedup mean: '+str(geomean_SNUMA_halfBW_normalized_IPCs))

plt.rcParams.update({'font.size': 14})
labelfontsize=18

#print(base_amats)
colors=['#dda0dd', '#8fbc8f', '#faa460', '#9370db','#FFCD70']
index = np.arange(len(benchmarks))
# Plot normalized IPCs
#plt.figure(figsize=(10, 4))
plt.figure(figsize=(10, 3))
barwidth=0.175

plt.bar(index-(barwidth*1.5), b_ISOBW_normalized_IPCs, width=barwidth, label='Baseline ISO-BW', color=colors[4], edgecolor='black')
plt.bar(index-(barwidth*0.5), b_2X_normalized_IPCs, width=barwidth, label='Baseline 2X BW', color=colors[2], edgecolor='black')

#plt.bar(index+(barwidth*0.5), SNUMA_halfBW_normalized_IPCs, label='StarNUMA'+r'$\frac{1}{2}$'+' BW' , width=barwidth, color='#DDA0DD')
plt.bar(index+(barwidth*0.5), SNUMA_halfBW_normalized_IPCs, label='StarNUMA'+' half BW' , width=barwidth, color='#DDA0DD', edgecolor='black')
plt.bar(index+(barwidth*1.5), normalized_IPCs, width=barwidth, label='StarNUMA', color='#9370db', edgecolor='black')
plt.axhline(1, color='black', linestyle='--')
plt.grid(axis='y', linestyle='--')
plt.gca().set_axisbelow(True)  # Set grid behind
#plt.xlabel('Benchmark')
plt.ylim([0,2.7])
plt.ylabel('Normalized IPC', fontsize=labelfontsize)
plt.xticks(index, benchmarks)
plt.legend(ncol=2, fontsize=13)
# Add annotation for geometric mean
mean_x_position = benchmarks.index('MEAN')
#plt.text(mean_x_position, geomean_normalized_IPCs + 0.02, f'{geomean_normalized_IPCs:.2f}', ha='center')
#plt.title('Normalized IPC for each benchmark')
#plt.xticks(rotation=90)
#plt.show()
plt.savefig('BW_sensitivity_IPC.png', bbox_inches='tight')
plt.savefig('BW_sensitivity_IPC.pdf', bbox_inches='tight')


