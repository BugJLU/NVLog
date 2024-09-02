import matplotlib.pyplot as plt
import numpy as np

fsnames = ['Base', 'NOVA', 'SPFS', 'NVLog']
# titles = ['Append', 'Seq Overwrite', 'Rand Overwrite']
titles = ['Ext-4', 'XFS']
sizes = ['100B', '1KB', '4KB', '16KB']

legends1 = ['Ext-4', 'NOVA', 'SPFS', 'NVLog']
legends2 = ['XFS', 'NOVA', 'SPFS', 'NVLog']
# legends2 = ['NVLog', 'NVLog-fsync']
# legends3 = ['NVLog-fsync +Optm']

data_nova={
    "100B": [9.71,9.71],
    "1K":   [138.33,138.33],
    "4K":   [840.33,840.33],
    "16K":  [1843,1843],
}

data_spfs={
    "100B": [20.06,     20.46],
    "1K":   [208.0,     192.0],
    "4K":   [456.66,    435.33],
    "16K":  [638.66,    1090.66],
}

data_base={
    "100B": [4.81,      4.79],
    "1K":   [29.76,     29.66],
    "4K":   [52.69,     52.13],
    "16K":  [178.66,    178.66],
}

data_nvpc={
    "100B": [40.1,      32.4],
    "1K":   [274.0,     230.33],
    "4K":   [795.33,    705.66],
    "16K":  [862.33,    800],
}




data_100B=[
    data_base.get("100B"),
    data_nova.get("100B"),
    data_spfs.get("100B"),
    data_nvpc.get("100B"),
]

data_1K=[
    data_base.get("1K"),
    data_nova.get("1K"),
    data_spfs.get("1K"),
    data_nvpc.get("1K"),
]

data_4K=[
    data_base.get("4K"),
    data_nova.get("4K"),
    data_spfs.get("4K"),
    data_nvpc.get("4K"),
]

data_16K=[
    data_base.get("16K"),
    data_nova.get("16K"),
    data_spfs.get("16K"),
    data_nvpc.get("16K"),
]

data = np.array([data_100B,data_1K,data_4K,data_16K])

colors = {
    'Base' : '#bd8066', 
    'NOVA' : '#8b9d4a', # BDD174
    'SPFS' : '#a56cb6', # BD97CE
    # 'P2CACHE (sim)' : '#64ae8f', # CCA391
    'NVLog' : '#5FA4D9', 
}

hatches = {
    'Base' : '//', 
    'NOVA' : '--', # BDD174
    'SPFS' : 'xx', # BD97CE
    # 'P2CACHE (sim)' : '#64ae8f', # CCA391
    'NVLog' : '\\\\', 
}

# plt.figure(figsize=(4, 2))

plt.rcParams['axes.titlesize'] = 14
plt.rcParams['xtick.labelsize'] = 'large'
plt.rcParams['ytick.labelsize'] = 'large'
plt.rcParams['hatch.color'] = 'white'

fig, axes = plt.subplots(1, 2, sharey=True, figsize=(8, 3), layout='constrained')
plt.subplots_adjust(wspace=0, hspace=0)

fig.supxlabel("I/O size")
fig.supylabel("Throughput (MB/s)")

for i, title in enumerate(titles):
    # plt.subplot(1, 3, i+1)
    # legend_handlers = []
    axes[i].set_title(title)
    this_data = data[:, :, i]
    for j, cluster in enumerate(fsnames):
        # print(data[:, j, i])
        b = axes[i].bar(np.arange(4)+5*j, this_data[j, :], color=list(colors.values()), hatch=list(hatches.values()), label=fsnames)
        # legend_handlers.append(b)
    
    # for k, fs in enumerate(fsnames):
    #     # for j, sz in enumerate(sizes):
    #     #     if fs == 'NVLog-fsync +Optm':
    #     #         b=axes[i].bar([j*(len(fsnames)+1)+k+0.5], this_data[j, k]-this_data[j, k-1], bottom=this_data[j, k-1], color=colors[fs], hatch=hatches[fs])
    #     #     else:
    #     #         b=axes[i].bar([j*(len(fsnames)+1)+k+1.5], this_data[j, k], color=colors[fs], hatch=hatches[fs])
    #     b=axes[i].bar(np.arange(4), this_data[:, k], color=colors[fs], hatch=hatches[fs])
    #     # if fs == 'NVLog':
    #     #     axes[i].plot(np.arange(len(sizes))*(len(fsnames)+1)+k+1.5, this_data[:, k], color='#3a5f7c', marker='.', linewidth=0.5)
    #     # if (title == 'Ext-4') and fs in legends1:
    #     #     legend_handlers.append(b)
    #     # if (title == 'XFS') and fs in legends2:
    #         # legend_handlers.append(b)
    #     legend_handlers.append(b)
    #     # if (title == 'Rand Overwrite') and fs in legends3:
    #     #     legend_handlers.append(b)
    axes[i].plot(np.arange(len(sizes))*(len(fsnames)+1)+3, this_data[:, 3], color='#3a5f7c', marker='.', linewidth=0.5)
    
    axes[i].set_xticks(np.arange(4)*(len(fsnames)+1)+(len(fsnames)-1)/2, sizes)
    axes[i].set_yscale('log')
    if (title == 'Ext-4'): 
        axes[i].legend(b, legends1, loc='upper left')#, fontsize='small', markerscale=0.5)
    if (title == 'XFS'):
        axes[i].legend(b, legends2, loc='upper left')#, fontsize='small', markerscale=0.5)
    # if (title == 'Rand Overwrite'):
    #     axes[i].legend(legend_handlers, legends3, loc='upper left')#, fontsize='small', markerscale=0.5)
    # plt.savefig('plot2_'+title.replace(' ', '').replace('/','-')+'.pdf', bbox_inches='tight')


plt.savefig('diffsizes.pdf', bbox_inches='tight')
