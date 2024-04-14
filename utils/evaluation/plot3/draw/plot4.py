import matplotlib.pyplot as plt
import numpy as np

fsnames = ['Ext-4 16D', 'Ext-4 48D', 'NVPC 16D32N']
fsnames_l = ['Ext-4 16GB DRAM', 'Ext-4 48GB DRAM', 'NVPC 16GB DRAM 32GB NVM']

results = {
    'Uniform' : [76.16, 261.66, 254], 
    'Zipfian' : [257.66, 844.33, 692.66]
}

cost_128N = 419
cost_128D = 877

costs = [
    cost_128D / 128 * 16, 
    cost_128D / 128 * 48, 
    cost_128D / 128 * 16 + cost_128N / 128 * 32
]

cost_eff = {
    'Uniform' : np.array(results['Uniform']) / np.array(costs),
    'Zipfian' : np.array(results['Zipfian']) / np.array(costs),
}



colors = ['#bd8066', '#a56cb6', '#5FA4D9']

hatches = ['/', '\\', '-']
# {
#     'Ext-4' : '//', 
#     'NOVA' : '--', # BDD174
#     'SPFS' : 'xx', # BD97CE
#     # 'P2CACHE (sim)' : '#64ae8f', # CCA391
#     'NVPC' : '\\\\', 
#     'NVPC-fsync' : '++', 
#     'NVPC-fsync +Optm' : '++'
# }


for t in results:
    # plt.rcParams['legend.fontsize'] = 'small'
    plt.rcParams['ytick.labelsize'] = 11
    plt.rcParams['hatch.color'] = 'white'
    fig = plt.figure(figsize=(3,2))
    fig_legend = plt.figure(figsize=(8, 0.25))
    
    ax1 = fig.subplots()
    ax2 = ax1.twinx()
    ax1.tick_params(axis='x', which='both', bottom=False, top=False, labelbottom=False) 
    ax2.tick_params(axis='x', which='both', bottom=False, top=False, labelbottom=False) 
    ax1.set_ylabel("Throughput (MB/s)", fontsize=12)
    ax2.set_ylabel("Performance per-$", fontsize=12)
    handler = ax1.bar(range(0, 3), results[t], color=colors, label=fsnames, width=0.5, hatch=hatches)
    ax2.plot(range(0, 3), cost_eff[t], color='black', marker='x')
    fig_legend.legend(handler, fsnames_l, ncol=3, loc='center', fontsize=12.5, markerscale=8, frameon=False, mode='expand')
    ax1.set_title(t, fontsize=14)
    fig.savefig('plot4_'+t.replace(' ', '_')+'.pdf', bbox_inches='tight')
    fig_legend.savefig('plot4_legends.pdf')
