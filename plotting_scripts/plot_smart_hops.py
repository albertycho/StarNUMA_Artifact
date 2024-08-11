import matplotlib.pyplot as plt
from matplotlib.patches import Patch
import csv
import numpy as np

lats=[80,130,360,180,413,280]

# Load the data
data = []
#with open('collected_hop_stats.csv', 'r') as file:
with open('collected_hop_stats.txt', 'r') as file:
    reader = csv.reader(file)
    headers = next(reader)  # Skip header
    data = list(reader)

benchmarks = [row[0] for row in data]

# Prepare the data for plotting
base_values = [[int(val) for val in row[1:7]] for row in data]
cxi_values = [[int(val) for val in row[7:13]] for row in data]

# Normalize the counts to PDFs
base_values = [[val/sum(group) for val in group] for group in base_values]
cxi_values = [[val/sum(group) for val in group] for group in cxi_values]

######## Append two empty entries #########################################
empty_entry = [0, 0, 0, 0, 0, 0]  # An entry with all zeros
base_values.append(empty_entry)
base_values.append(empty_entry)
cxi_values.append(empty_entry)
cxi_values.append(empty_entry)
benchmarks.append('')
benchmarks.append('')
##############################################################################

bar_width = 0.22  # The width of the bars
spacing = 0.02  # The spacing between bars
n_benchmarks = len(benchmarks)
indices = np.arange(n_benchmarks)

# Colors for the bars
#colors = ['tab:green', 'tab:blue', 'tab:red', 'tab:purple']
#colors=['#dda0dd', '#8fbc8f', '#faa460', '#9370db']
#colors=['#dda0dd', '#8fbc8f', '#faa460', '#9370db','lightyellow','aliceblue']
#colors=['#dda0dd', '#8fbc8f', '#faa460', '#9370db','lightyellow','#F59B8B']
colors=['#dda0dd', '#8fbc8f', '#faa460', '#9370db','lightyellow','#BB979C']
plt.rcParams.update({'font.size': 14})
labelfontsize=18

#plt.figure(figsize=(10,4))
plt.figure(figsize=(10,3))
#fig, plt = plt.subplots(figsize=(10, 4))  
base_amats=[0]*len(base_values)
cxi_amats =[0]*len(cxi_values)

for i, lat in enumerate(lats):
    print(lat)
    base_amats = [a+(b*lat) for a,b in zip(base_amats, [base[i] for base in base_values])]
    cxi_amats = [a+(b*lat) for a,b in zip(cxi_amats, [cxi[i] for cxi in cxi_values])]

###################### remove extra 2 entries of 0's I added ################
base_amats=base_amats[:len(base_amats)-2]
cxi_amats=cxi_amats[:len(cxi_amats)-2]
############################################################################
# Writing to a file
with open('amat_data.csv', 'w', newline='') as file:
    writer = csv.writer(file)
    writer.writerow(['estimated_base_amats'] + base_amats)  # Adding a header for clarity
    writer.writerow(['estimated_cxi_amats'] + cxi_amats)


print(base_amats)
print(cxi_amats)
base_amat_mean = np.mean(base_amats)
cxi_amat_mean = np.mean(cxi_amats)
amat_reduction = cxi_amat_mean / base_amat_mean
print(amat_reduction)

twohop_ratios=[]
#print(base_values)
for bv in base_values:
#    print(bv[2])
    twohop_ratios.append(bv[2])

print(twohop_ratios)
twohoprat_mean = np.mean(twohop_ratios)
print(twohoprat_mean)

ctwohop_ratios=[]
#print(base_values)
for cv in cxi_values:
#    print(bv[2])
    ctwohop_ratios.append(cv[3])

print(ctwohop_ratios)
ctwohoprat_mean = np.mean(ctwohop_ratios)
print(ctwohoprat_mean)

# Plot base bars (stacked bars)
bottom_values = [0] * len(base_values)
for i, color in enumerate(colors):
    plotvals = [base[i] for base in base_values]
    plt.bar(indices - bar_width * 0.75, plotvals, bar_width, color=color, alpha=1, bottom=bottom_values, edgecolor='black')
    bottom_values = [a + b for a, b in zip(plotvals, bottom_values)]

# Plot cxi bars
cbottom_values = [0] * len(cxi_values)
for i, color in enumerate(colors):
    plotvals = [cxi[i] for cxi in cxi_values]
    plt.bar(indices + bar_width * 0.75, plotvals, bar_width, color=color, hatch='//', alpha=1, bottom=cbottom_values, edgecolor='black')
    cbottom_values = [a + b for a, b in zip(plotvals, cbottom_values)]
# Adjust layout to prevent overlap
plt.tight_layout()
plt.subplots_adjust(left=0.2) 
plt.ylim(0, 1.05)


plt.xticks(indices, benchmarks,rotation=0)

plt.ylabel('Access Distribution', fontsize=labelfontsize)

# Grid and layout adjustments
plt.grid(True, which='both', axis='y', linestyle='--', linewidth=0.5, zorder=0)

# Create and set legend
# ...
legend_elements = [Patch(facecolor=color, label=headers[i+1].split('_')[-1], edgecolor='black') for i, color in enumerate(colors)]
legend_elements[3] = Patch(facecolor=legend_elements[3].get_facecolor(), label='Pool', edgecolor='black')
legend_elements[0] = Patch(facecolor=legend_elements[0].get_facecolor(), label='Local', edgecolor='black')
legend_elements[4] = Patch(facecolor=legend_elements[4].get_facecolor(), label='BT_Socket', edgecolor='black')
legend_elements[5] = Patch(facecolor=legend_elements[5].get_facecolor(), label='BT_Pool', edgecolor='black')
legend_elements.append(Patch(facecolor='white', label='Baseline',edgecolor='black'))
legend_elements.append(Patch(facecolor='white', label='StarNUMA', hatch='//',edgecolor='black'))

#plt.legend(handles=legend_elements, ncol=4, loc='upper center', bbox_to_anchor=(0.5,1.25))
plt.legend(handles=legend_elements, ncol=1, loc='best', fontsize=11)

# Save the plot
plt.savefig('accessdist.png', bbox_inches='tight')
plt.savefig('accessdist.pdf', bbox_inches='tight')

exit(0)

