import matplotlib.pyplot as plt
import numpy as np

fsnames = ['Ext-4', 'NOVA', 'SPFS', 'NVPC']

data = {
    'Ext-4': [382.66, 743.66, 1440.0, 2631.33, 3449.33],
    'NOVA': [881.33, 1570.0, 2591.0, 3710.33, 3350.33],
    'SPFS': [485.0, 636.33, 752.0, 813.0, 790.0],
    'NVPC': [1189.33, 2311.0, 4462.66, 7212.66, 4971.0]
}

colors = {
    'Ext-4' : '#bd8066', 
    'NOVA' : '#8b9d4a', # BDD174
    'SPFS' : '#a56cb6', # BD97CE
    # 'P2CACHE (sim)' : '#64ae8f', # CCA391
    'NVPC' : '#5FA4D9', 
    'NVPC-fsync' : '#64ae8f', 
    'NVPC-fsync +Optm' : '#83f3c4'
}

markers = {
    'Ext-4' : '^',
    'NOVA' : 'v',
    'SPFS' : 's',
    'P2CACHE (sim)' : 'D',
    'NVPC' : 'o',
}

plt.figure(figsize=(5,2.5))
plt.xlabel("Thread number")
plt.ylabel("Throughput (MB/s)")
plt.xticks(ticks=range(0, 5), labels=['1', '2', '4', '8', '16'])
# plt.ylim(top=7500)
# plt.title(title)
x = range(0, 5)
for fs in fsnames:
    plt.plot(x, data[fs], marker=markers[fs], color=colors[fs], label=fs)

plt.legend()
plt.savefig('plot3.pdf', bbox_inches='tight')