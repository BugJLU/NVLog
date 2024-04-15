import matplotlib.pyplot as plt
import numpy as np

fsnames = ['Ext-4', 'NOVA', 'SPFS', 'NVPC', 'NVPC-fsync', 'NVPC-fsync +Optm']
titles = ['Append', 'Seq Overwrite', 'Rand Overwrite']
sizes = ['100B', '1KB', '4KB', '16KB']

legends1 = ['Ext-4', 'NOVA', 'SPFS']
legends2 = ['NVPC', 'NVPC-fsync']
legends3 = ['NVPC-fsync +Optm']

data_nova={
    "100B":[9.73,9.71,8.85],
    "1K":[138.33,138.33,116.0],
    "4K":[843,840.33,752.66],
    "16K":[1811,1843,1777.66]
}

data_spfs={
    "100B":[20.16,20.06,19.0],
    "1K":[210.0,208.0,180.0],
    "4K":[453.0,456.66,413.66],
    "16K":[638.0,638.66,623.0]
}

data_ext4={
    "100B":[4.81, 4.81, 4.87],
    "1K":[29.7, 29.76, 28.7],
    "4K":[52.5, 52.69, 48.53],
    "16K":[178.66, 178.66, 168.33]
}

data_nvpc={
    "100B":[40.76,40.1,37.43],
    "1K":[272.33,274.0,244.0],
    "4K":[797.33,795.33,731.0],
    "16K":[863.0,862.33,844.33]
}


data_nvpc_active={
    "100B":[29.46, 29.46, 27.59],
    "1K":[219.66, 223.33, 202.0],
    "4K":[672.33, 670.0, 628.66],
    "16K":[806.0, 808.66, 783.66]
}


data_nvpc_inactive={
    "100B":[22.0, 22.2, 17.3],
    "1K":[206.66, 207.66, 172.66],
    "4K":[667.0, 662.66, 619.66],
    "16K":[806.66, 805.0, 785.33]
}

data_100B=[
    data_ext4.get("100B"),
    data_nova.get("100B"),
    data_spfs.get("100B"),
    data_nvpc.get("100B"),
    data_nvpc_inactive.get("100B"),
    data_nvpc_active.get("100B"),
]

data_1K=[
    data_ext4.get("1K"),
    data_nova.get("1K"),
    data_spfs.get("1K"),
    data_nvpc.get("1K"),
    data_nvpc_inactive.get("1K"),
    data_nvpc_active.get("1K")
]

data_4K=[
    data_ext4.get("4K"),
    data_nova.get("4K"),
    data_spfs.get("4K"),
    data_nvpc.get("4K"),
    data_nvpc_inactive.get("4K"),
    data_nvpc_active.get("4K")
]

data_16K=[
    data_ext4.get("16K"),
    data_nova.get("16K"),
    data_spfs.get("16K"),
    data_nvpc.get("16K"),
    data_nvpc_inactive.get("16K"),
    data_nvpc_active.get("16K")
]

data = np.array([data_100B,data_1K,data_4K,data_16K])

colors = {
    'Ext-4' : '#bd8066', 
    'NOVA' : '#8b9d4a', # BDD174
    'SPFS' : '#a56cb6', # BD97CE
    # 'P2CACHE (sim)' : '#64ae8f', # CCA391
    'NVPC' : '#5FA4D9', 
    'NVPC-fsync' : '#64ae8f', 
    'NVPC-fsync +Optm' : '#83f3c4'
}

hatches = {
    'Ext-4' : '//', 
    'NOVA' : '--', # BDD174
    'SPFS' : 'xx', # BD97CE
    # 'P2CACHE (sim)' : '#64ae8f', # CCA391
    'NVPC' : '\\\\', 
    'NVPC-fsync' : '++', 
    'NVPC-fsync +Optm' : '++'
}

# plt.figure(figsize=(4, 2))

plt.rcParams['axes.titlesize'] = 14
plt.rcParams['xtick.labelsize'] = 'large'
plt.rcParams['ytick.labelsize'] = 'large'
plt.rcParams['hatch.color'] = 'white'

fig, axes = plt.subplots(1, 3, sharey=True, figsize=(12, 3), layout='constrained')
plt.subplots_adjust(wspace=0, hspace=0)

fig.supxlabel("I/O size")
fig.supylabel("Throughput (MB/s)")

for i, title in enumerate(titles):
    # plt.subplot(1, 3, i+1)
    legend_handlers = []
    axes[i].set_title(title)
    this_data = data[:, :, i]
    # for j, cluster in enumerate(fsnames):
    #     # print(data[:, j, i])
    #     plt.bar(np.arange(4)+1, this_data[:, j], width=bar_width)
    
    for k, fs in enumerate(fsnames):
        for j, sz in enumerate(sizes):
            if fs == 'NVPC-fsync +Optm':
                b=axes[i].bar([j*(len(fsnames)+1)+k+0.5], this_data[j, k]-this_data[j, k-1], bottom=this_data[j, k-1], color=colors[fs], hatch=hatches[fs])
            else:
                b=axes[i].bar([j*(len(fsnames)+1)+k+1.5], this_data[j, k], color=colors[fs], hatch=hatches[fs])
            
        if fs == 'NVPC':
            axes[i].plot(np.arange(len(sizes))*(len(fsnames)+1)+k+1.5, this_data[:, k], color='#3a5f7c', marker='.', linewidth=0.5)
        if (title == 'Append') and fs in legends1:
            legend_handlers.append(b)
        if (title == 'Seq Overwrite') and fs in legends2:
            legend_handlers.append(b)
        if (title == 'Rand Overwrite') and fs in legends3:
            legend_handlers.append(b)
    
    axes[i].set_xticks(np.arange(4)*(len(fsnames)+1)+(len(fsnames)+1)/2, sizes)
    axes[i].set_yscale('log')
    if (title == 'Append'):
        axes[i].legend(legend_handlers, legends1, loc='upper left')#, fontsize='small', markerscale=0.5)
    if (title == 'Seq Overwrite'):
        axes[i].legend(legend_handlers, legends2, loc='upper left')#, fontsize='small', markerscale=0.5)
    if (title == 'Rand Overwrite'):
        axes[i].legend(legend_handlers, legends3, loc='upper left')#, fontsize='small', markerscale=0.5)
    # plt.savefig('plot2_'+title.replace(' ', '').replace('/','-')+'.pdf', bbox_inches='tight')


plt.savefig('plot2.pdf', bbox_inches='tight')
