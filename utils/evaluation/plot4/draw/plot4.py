import matplotlib.pyplot as plt
import numpy as np

fsnames = ['NOVA', 'Ext-4', 'SPFS/Ext-4', 'NVLog/Ext-4', 'XFS', 'SPFS/XFS', 'NVLog/XFS']

data = {
    'NOVA': [881.33, 1570.0, 2591.0, 3710.33, 3350.33],
    'Ext-4': [382.66, 743.66, 1440.0, 2631.33, 3449.33],
    'SPFS/Ext-4': [485.0, 636.33, 752.0, 813.0, 790.0],
    'NVLog/Ext-4': [1189.33, 2311.0, 4462.66, 7212.66, 4971.0],
    'XFS' : [392.66, 767, 1483, 2717.66, 3534.33], 
    'SPFS/XFS' : [57.80, 104.23, 168.76, 251.66, 375.0],
    'NVLog/XFS' : [1150.33, 2250, 4267.33, 7092, 4889], 
}

colors = {
    'NOVA' : '#8b9d4a', # BDD174
    'Ext-4' : '#bd8066', 
    'SPFS/Ext-4' : '#a56cb6', # BD97CE
    # 'P2CACHE (sim)' : '#64ae8f', # CCA391
    'NVLog/Ext-4' : '#5FA4D9', 
    'XFS' : '#64ae8f', 
    'SPFS/XFS' : '#EEC900',
    'NVLog/XFS' : '#ba3e3e', 

}

markers = {
    'NOVA' : 'v',
    'Ext-4' : '^',
    'SPFS/Ext-4' : 's',
    # 'P2CACHE (sim)' : 'D',
    'NVLog/Ext-4' : 'o',
    'XFS' : '1',
    'SPFS/XFS' : '+',
    'NVLog/XFS' : 'x',
}

lines = {
    'NOVA' : '-',
    'Ext-4' : '-',
    'SPFS/Ext-4' : '-',
    # 'P2CACHE (sim)' : 'D',
    'NVLog/Ext-4' : '-',
    'XFS' : '--',
    'SPFS/XFS' : '--',
    'NVLog/XFS' : '--',
}

plt.figure(figsize=(8,3.5))
plt.xlabel("Thread number")
plt.ylabel("Throughput (MB/s)")
plt.xticks(ticks=range(0, 5), labels=['1', '2', '4', '8', '16'])
# plt.ylim(top=7500)
# plt.title(title)
x = range(0, 5)
for fs in fsnames:
    plt.plot(x, data[fs], marker=markers[fs], markeredgewidth=1, markerfacecolor='none', linestyle=lines[fs], color=colors[fs], label=fs)

plt.legend()
plt.savefig('scale.pdf', bbox_inches='tight')