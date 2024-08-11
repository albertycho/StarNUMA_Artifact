from scipy.stats import gmean
import csv
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Patch
# Open the file and read the data
with open('collected_smart_stats.csv', 'r') as csvfile:
#with open('collected_smart_stats.csv', 'r') as csvfile:
    reader = csv.reader(csvfile)
    next(reader)  # Skip the header
    benchmarks = []
    normalized_IPCs = []
    normalized_IPCs_SH = []
    base_amats = []
    cxi_amats = []
    for row in reader:
        print(row)
        #if len(row) == 5:  # Make sure we have a full row of data
        benchmarks.append(row[0])
        base_IPC = float(row[1])
        cxi_IPC = float(row[2])
        cxi_IPC_SH = float(row[5])
        normalized_IPCs.append(cxi_IPC / base_IPC if base_IPC else 0)  # Avoid division by zero
        normalized_IPCs_SH.append(cxi_IPC_SH / base_IPC if base_IPC else 0)  # Avoid division by zero
        base_amats.append(float(row[3]))
        cxi_amats.append(float(row[4]))


try:
    with open('amat_data.csv', 'r') as file:
        reader = csv.reader(file)
        estimated_base_amats = []
        estimated_cxi_amats = []
        for row in reader:
            if row[0] == 'estimated_base_amats':
                print('estimated_base_amats find')
                estimated_base_amats = [float(val) for val in row[1:]]
            elif row[0] == 'estimated_cxi_amats':
                estimated_cxi_amats = [float(val) for val in row[1:]]

        # Check if the lists were populated
        if not estimated_base_amats or not estimated_cxi_amats:
            raise ValueError("Data not found in the file")

except (FileNotFoundError, ValueError) as e:
    print("Warning: Unable to read the file or data not found. Creating dummy data.")
    # Create dummy data
    #estimated_base_amats = [0.0, 0.0, 0.0]  # Example dummy data
    estimated_base_amats = [0.0]*len(base_amats)
    #estimated_cxi_amats = [0.0, 0.0, 0.0]   # Adjust as needed
    estimated_cxi_amats = [0.0]*len(cxi_amats)

# Calculate the geometric mean for normalized IPCs
geomean_normalized_IPCs = gmean(normalized_IPCs)
geomean_normalized_IPCs_SH = gmean(normalized_IPCs_SH)
# Calculate the arithmetic mean for AMATs
arithmetic_mean_base_amats = np.mean(base_amats)
arithmetic_mean_cxi_amats = np.mean(cxi_amats)

arithmetic_mean_estimated_base_amats = np.mean(estimated_base_amats)
arithmetic_mean_estimated_cxi_amats = np.mean(estimated_cxi_amats)

print('base mean amat: '+str(arithmetic_mean_base_amats))
print('snuma mean amat: '+str(arithmetic_mean_cxi_amats))


# adding an empty slot before mean
benchmarks.append('')
normalized_IPCs.append(0)
normalized_IPCs_SH.append(0)
base_amats.append(0)
cxi_amats.append(0)
estimated_base_amats.append(0)
estimated_cxi_amats.append(0)


base_amats.append(arithmetic_mean_base_amats)
cxi_amats.append(arithmetic_mean_cxi_amats)
estimated_base_amats.append( arithmetic_mean_estimated_base_amats)
estimated_cxi_amats.append(  arithmetic_mean_estimated_cxi_amats )

print('estimated_base_amats length  : '+str(len(estimated_base_amats)))
print('base_amats length            : '+str(len(base_amats)))

print('estimated_cxi_amats length  : '+str(len(estimated_cxi_amats)))
print('cxi_amats length            : '+str(len(cxi_amats)))



benchmarks.append('MEAN')
normalized_IPCs.append(geomean_normalized_IPCs)
normalized_IPCs_SH.append(geomean_normalized_IPCs_SH)
print("SH_speedup: "+str(geomean_normalized_IPCs_SH))
print("CA_speedup: "+str(geomean_normalized_IPCs))


plt.rcParams.update({'font.size': 14})
labelfontsize=18

print(base_amats)

# Plot normalized IPCs
plt.figure(figsize=(10, 3))
barwidth=0.5
plt.bar(benchmarks, normalized_IPCs, width=0.5, color='#9370db',edgecolor='black', alpha=1, label=r'T$_{16}$')
plt.bar(benchmarks, normalized_IPCs_SH, width=0.5, color='white',edgecolor='black', alpha=1)
plt.bar(benchmarks, normalized_IPCs_SH, width=0.5, color='#9370db',edgecolor='black', alpha=0.7, label=r'T$_{0}$', hatch='/')
plt.axhline(1, color='black', linestyle='--')
plt.grid(axis='y', linestyle='--')
plt.gca().set_axisbelow(True)  # Set grid behind
plt.legend(ncol=2)
#plt.xlabel('Benchmark')
plt.ylabel('Normalized IPC', fontsize=labelfontsize)
# Add annotation for geometric mean
mean_x_position = benchmarks.index('MEAN')
plt.text(mean_x_position, geomean_normalized_IPCs + 0.02, f'{geomean_normalized_IPCs:.2f}', ha='center')
#plt.text(mean_x_position, geomean_normalized_IPCs_SH - 0.2, f'{geomean_normalized_IPCs_SH:.2f}', ha='center')
#plt.title('Normalized IPC for each benchmark')
#plt.xticks(rotation=90)
#plt.show()
plt.savefig('main_IPC.png', bbox_inches='tight')
plt.savefig('main_IPC.pdf', bbox_inches='tight')

colors =['#ce4257','#FFA500']
#colors =['#ce4257','#80580F']

# Plot AMATs
plt.figure(figsize=(10, 3))
bar_width = 0.22
index = range(len(benchmarks))

plt.bar([i-bar_width*0.75 for i in index], base_amats, bar_width, label='Baseline', color=colors[0], edgecolor='black')
plt.bar([i + bar_width*0.75 for i in index], cxi_amats, bar_width, label='StarNUMA', color=colors[0], hatch='//', edgecolor='black')

plt.bar([i-bar_width*0.75 for i in index], estimated_base_amats, bar_width, label='Baseline', color=colors[1],  edgecolor='black')
plt.bar([i + bar_width*0.75 for i in index], estimated_cxi_amats, bar_width, label='StarNUMA', color=colors[1], hatch='//', edgecolor='black')


plt.grid(axis='y', linestyle='--')
plt.gca().set_axisbelow(True)  # Set grid behind

plt.ylim([0,1100])
plt.yticks(fontsize=12)
#yticks, yticklabels = plt.yticks()
#yticklabels = [label.get_text() for label in yticklabels]
#yticklabels=['0','200','400','600','800','1000']
#plt.yticks(ticks=yticks, labels=yticklabels)

#plt.xlabel('Benchmark')
plt.ylabel('AMAT (ns)', fontsize=labelfontsize)
#plt.title('AMAT for each benchmark')
#plt.xticks([i + bar_width / 2 for i in index], benchmarks)
plt.xticks(index, benchmarks)
#plt.xticks([i + bar_width / 2 for i in index], benchmarks, rotation=90)

# Create custom legend
legend_elements = [
    #Patch(facecolor=colors[1], label='Expected AMAT', edgecolor='black'),
    Patch(facecolor=colors[1], label='Unloaded Latency', edgecolor='black'),
    Patch(facecolor=colors[0], label='Contention Delay', edgecolor='black'),
    Patch(facecolor='white', label='Baseline', edgecolor='black'),
    Patch(facecolor='white', label='StarNUMA', hatch='//', edgecolor='black')
]

# Add the legend to the plot
plt.legend(handles=legend_elements, loc='upper right', ncol=2)

#plt.legend()
#plt.show()
plt.savefig('main_eval_AMAT.png', bbox_inches='tight')
plt.savefig('main_evalAMAT.pdf', bbox_inches='tight')



