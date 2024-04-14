import matplotlib.pyplot as plt
import numpy as np

data = {
    'SeqRead'   : [2899.33, 2721, 1355.66, 4277.33, 2435, 4198.66], #, 0],
    'SeqWrite'  : [1781.66, 1171.33, 1901.66, 2656.33, 1920, 2632.33, 185.33],
    'RandRead'  : [1271.33, 1585.66, 844, 2572, 57.03, 2573.33], #, 0],
    'RandWrite' : [695.66, 554.66, 1244, 1550.66, 1214.33, 1514.66, 47.69],
}

fsnames = ['NOVA', 'Ext-4-DAX', 'Ext-4.NVM.C', 'Ext-4.NVM.H', 'Ext-4.SSD.C', 'Ext-4.SSD.H', 'Ext-4.SSD.S']

colors = ['#5FA4D9', '#BDD174', '#CCA391', '#665148', '#BD97CE', '#5D4A66', '#BD97CE']

hatches = ['-', '/', '+', '--', 'x', '//', '\\\\']

plt.rcParams['hatch.color'] = 'white'
plt.figure(figsize=(10, 4))
plt.ylim((0, 3500))

idx = 0
ticks_pos = []
h = []

for i, clust in enumerate(data):
    h=plt.bar(np.arange(len(data[clust]))+idx, data[clust], color=colors[:len(data[clust])], hatch=hatches[:len(data[clust])], width=1)
    ticks_pos.append(idx+len(data[clust])/2)
    idx += len(data[clust])+1

plt.text(3, 3500, "4277.33", ha='center', va='bottom', rotation=30)
plt.text(5, 3500, "4198.66", ha='center', va='bottom', rotation=30)
plt.text(13, 185.33, "185.33", ha='center', va='bottom', rotation=60, fontsize='small')
plt.text(19, 57.03, "57.03", ha='center', va='bottom', rotation=60, fontsize='small')
plt.text(28, 47.69, "47.69", ha='center', va='bottom', rotation=60, fontsize='small')


plt.xticks(ticks_pos, list(data.keys()))
plt.ylabel("Throughput (MB/s)")
plt.legend(h, fsnames, ncol=3)
plt.savefig('plot0.pdf', bbox_inches='tight')


# plt.legend(fontsize=22, ncol=2)