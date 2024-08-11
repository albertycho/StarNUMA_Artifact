import pandas as pd
import matplotlib.pyplot as plt
from io import StringIO


# Read the contents of the file into the csv_data string
with open('collected_smart_stats.txt', 'r') as file:
    csv_data = file.read()

# Now you can use csv_data as needed, for example, loading it into a DataFrame
import pandas as pd
from io import StringIO

# Load the CSV data into a DataFrame
df = pd.read_csv(StringIO(csv_data))


plt.rcParams.update({'font.size': 14})
labelfontsize=20


# Load the data into a DataFrame
#df = pd.read_csv(StringIO(csv_data))

# Normalize the columns
df['base_IPC_norm'] = df['base_IPC'] / df['base_IPC_main']
df['cxi_IPC_main_norm'] = df['cxi_IPC_main'] / df['base_IPC_main']
df['cxi_IPC_norm'] = df['cxi_IPC'] / df['base_IPC_main']

colors=['#dda0dd', '#8fbc8f', '#faa460', '#9370db','#FFCD70']

# Create a bar plot
fig, ax = plt.subplots(figsize=(10, 4))

bar_width = 0.2
index = df.index

bar1 = ax.bar(index - bar_width, df['base_IPC_norm'], bar_width, label='Baseline Static',  color=colors[4], edgecolor='black')
bar2 = ax.bar(index, df['cxi_IPC_main_norm'], bar_width, label='StarNUMA (V-A)', color='#9370db', edgecolor='black')
bar3 = ax.bar(index + bar_width, df['cxi_IPC_norm'], bar_width, label='StarNUMA Static', color='#DDA0DD', edgecolor='black')
ax.yaxis.grid(True)
#ax.grid()
#ax.set_xlabel('Benchmark')
ax.set_ylabel('Normalized IPC')
#ax.set_title('Normalized IPC for each Benchmark')
ax.set_xticks(index)
ax.set_xticklabels(df['benchmark'])
ax.legend(loc='upper right')

plt.tight_layout()
#plt.show()
plt.savefig('static_ipc.png')
plt.savefig('static_ipc.pdf')
