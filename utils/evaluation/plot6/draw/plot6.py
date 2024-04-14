import matplotlib.pyplot as plt
import numpy as np

fsnames = ['Ext-4', 'SPFS', 'NOVA', 'NVPC'] # P2CACHE
clusters = ['Fillseq', 'Readseq', 'R.rand.w.rand\n(small)', 'R.rand.w.rand\n(large)']

data_s = {
    'Fillseq':                  [10952.66,	63857.33,	47443.66,	57283.33],
    'Readseq':                  [282103.66,	280035,	    233596,	    286742.33],
    'R.rand.w.rand\n(small)':      [118711.66,	164143,	    131666,	    163866.33],
    'R.rand.w.rand\n(large)':      [37975.66,  46739.33,   88200,      98469]
}
data_l = {
    'Fillseq':                  [10952.66,	63857.33,	47443.66,	57283.33],
    'Readseq':                  [282103.66,	280035,	    233596,	    286742.33],
    'Readrandomwriterandom':    [118711.66,	164143,	    131666,	    163866.33],
}

data = {
    # 'Small': data_s, 
    # 'Large': data_l
    'all': data_s
}

colors = [
    '#bd8066', 
    '#a56cb6', 
    '#8b9d4a', 
    '#5FA4D9', 
]
hatches = [
    '//', 
    'xx', 
    '--', 
    '\\\\', 
]

for d in data:
    plt.rcParams['hatch.color'] = 'white'
    plt.figure(figsize=(6, 3))
    plt.ylabel("Throughput (Ops/s)")
    plt.xticks(np.arange(4)*5+1.5, clusters)
    plt.ticklabel_format(axis='y', style='sci', scilimits=(0,0))
    # plt.title(d)
    for i, clust in enumerate(clusters):
        h = plt.bar(np.arange(4)+5*i, data[d][clust], color=colors, hatch=hatches, label=fsnames)
    
    plt.legend(h, fsnames)
    plt.savefig('plot6_'+d+'.pdf', bbox_inches='tight')