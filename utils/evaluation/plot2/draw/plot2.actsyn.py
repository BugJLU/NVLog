import matplotlib.pyplot as plt
import numpy as np

fsnames = ['NOVA', 'NVLog', 'NVLog+ActiveSync', 'NVLog (O_SYNC)']
# titles = ['Append', 'Seq Overwrite', 'Rand Overwrite']
titles = ['Ext-4', 'XFS']
sizes = ['64B', '256B', '1KB', '4KB']

# legends1 = ['Ext-4', 'NOVA', 'SPFS', 'NVLog']
# legends2 = ['XFS', 'NOVA', 'SPFS', 'NVLog']
# legends2 = ['NVLog', 'NVLog-fsync']
# legends3 = ['NVLog-fsync +Optm']

data_nova={
    "64B":  [7.32,      7.32],
    "256B": [29.53,     29.53],
    "1KB":  [130.66,    130.66],
    "4KB":  [750.66,    750.66],
}

data_nvlog={
    "64B":  [14.73,     14.53],
    "256B": [57.6,      57.2],
    "1KB":  [207.33,    204.33],
    "4KB":  [640.33,    642.66],
}

data_nvlogas={
    "64B":  [23.5,      23.6],
    "256B": [74,        73.7],
    "1KB":  [214,       213.33],
    "4KB":  [656.66,    652.66],
}

data_osync={
    "64B":  [27.26,     25.96],
    "256B": [82.33,     80.43],
    "1KB":  [231,       233.33],
    "4KB":  [697.33,    736.33],
}




data_64B=[
    data_nova.get("64B"),
    data_nvlog.get("64B"),
    data_nvlogas.get("64B"),
    data_osync.get("64B"),
]

data_256B=[
    data_nova.get("256B"),
    data_nvlog.get("256B"),
    data_nvlogas.get("256B"),
    data_osync.get("256B"),
]

data_1KB=[
    data_nova.get("1KB"),
    data_nvlog.get("1KB"),
    data_nvlogas.get("1KB"),
    data_osync.get("1KB"),
]

data_4KB=[
    data_nova.get("4KB"),
    data_nvlog.get("4KB"),
    data_nvlogas.get("4KB"),
    data_osync.get("4KB"),
]

data = np.array([data_64B,data_256B,data_1KB,data_4KB])

colors = {
    
    'NOVA' : '#8b9d4a', # BDD174
    
    # 'P2CACHE (sim)' : '#64ae8f', # CCA391
    'NVLog' : '#64ae8f', 
    'NVLog+ActiveSync' : '#5FA4D9', 
    'NVLog (O_SYNC)' : '#a56cb6', # BD97CE
}

hatches = {
    
    'NOVA' : '--', # BDD174
    
    # 'P2CACHE (sim)' : '#64ae8f', # CCA391
    'NVLog' : '\\\\', 
    'NVLog+ActiveSync' : '//', 
    'NVLog (O_SYNC)' : 'xx', # BD97CE
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
    for j, size in enumerate(sizes):
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
    
    axes[i].set_xticks(np.arange(4)*(len(fsnames)+1)+(len(fsnames)-1)/2, sizes)
    axes[i].set_yscale('log')
    # if (title == 'Ext-4'): 
    #     axes[i].legend(b, legends1, loc='upper left')#, fontsize='small', markerscale=0.5)
    # if (title == 'XFS'):
    axes[i].legend(b, fsnames, loc='upper left')#, fontsize='small', markerscale=0.5)
    # if (title == 'Rand Overwrite'):
    #     axes[i].legend(legend_handlers, legends3, loc='upper left')#, fontsize='small', markerscale=0.5)
    # plt.savefig('plot2_'+title.replace(' ', '').replace('/','-')+'.pdf', bbox_inches='tight')


plt.savefig('actsyn.pdf', bbox_inches='tight')
