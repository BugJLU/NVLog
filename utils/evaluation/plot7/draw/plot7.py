import matplotlib.pyplot as plt
import numpy as np

fsnames = ['Ext-4', 'NOVA', 'NVLog'] # P2CACHE
clusters = ['A', 'B', 'C', 'D', 'E', 'F'] # 'R.rand.w.rand\n(small)', 'R.rand.w.rand\n(large)'


data_a = {
    'A':    [6534.95, 9359.31, 12489.36],
    'B':    [19282.36, 21056, 22609.3],
    'C':    [25357.6, 24679.56, 25412.76],
    'D':    [24164.93, 30984.66, 33910.6],
    'E':    [953.748, 1006.24, 959.586],
    'F':    [5778.26, 8026.19, 10103.36],
}

data = {
    # 'Small': data_s, 
    # 'Large': data_l
    'all': data_a
}

colors = [
    '#8b9d4a',
    # '#bd8066', 
    '#a56cb6', 
    '#5FA4D9',
    # '#5FA4D9', 
]
hatches = [
    '--', 
    # '//', 
    'xx', 
    '//', 
    # '\\\\', 
]

for d in data:
    plt.rcParams['hatch.color'] = 'white'
    plt.figure(figsize=(5, 2.5))
    plt.ylabel("Throughput (Ops/s)")
    plt.xticks(np.arange(6)*4+1, clusters)
    plt.ticklabel_format(axis='y', style='sci', scilimits=(0,0))
    # plt.title(d)
    for i, clust in enumerate(clusters):
        h = plt.bar(np.arange(3)+4*i, data[d][clust], color=colors, hatch=hatches, label=fsnames)
    
    plt.legend(h, fsnames)
    plt.savefig('ycsb-sqlite.pdf', bbox_inches='tight')