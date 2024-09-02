import matplotlib.pyplot as plt

fsnames = ['XFS', 'NOVA', 'SPFS', 'NVLog (AS)', 'NVLog']
legends1 = ['XFS', 'NOVA', 'SPFS']
legends2 = ['NVLog (AS)', 'NVLog']


data_0_10 = {
    fsnames[0]: [2800.61,	864.02,	    517.90,	    368.50,	    286.46,	    233.54],
    fsnames[1]: [813.98,    820.09,    824.39,    803.81,     822.14,    811.25 ],
    fsnames[2]: [2692.50,   18.93,     18.84,     18.85,     18.78,     18.59],
    fsnames[3]: [1005.18,   1003.19,    1004.01,    1005.48,    1005.57,    1006.47],
    fsnames[4]: [2652.91,	1858.51,	1491.99,	1256.44,	1105.24,	992.42]
}

data_3_7 = {
    fsnames[0]: [2799.09,	1078.12,	676.97,	    491.11,	    387.41,	    320.21],
    fsnames[1]: [888.26,    897.36,    896.16,    917.32,     906.07,    904.95 ],
    fsnames[2]: [2730.73,   13.79,     13.71,     13.61,     13.76,     13.73],
    fsnames[3]: [1192.46,   1189.31,    1190.37,    1192.11,    1191.35,    1197.55],
    fsnames[4]: [2734.37,	2045.28,	1709.06,	1485.85,	1313.66,	1192.80]
}

data_5_5 = {
    fsnames[0]: [2871.09,	1316.81,	857.04,	    638.17,	    507.68,	    424.7],
    fsnames[1]: [1010.24,   973.25,    1006.11,   1001.85,    1000.38,   987.14 ],
    fsnames[2]: [2817.07,   10.63,     10.89,     10.37,     10.52,     10.51],
    fsnames[3]: [1397.40,   1395.57,     1403.90,    1404.27,    1398.51,    1403.94],
    fsnames[4]: [2729.74,	2247.36,	1948.63,	1716.74,	1547.62,	1409.17]
}

data_7_3 = {
    fsnames[0]: [2852.5,	1663.24,	1188.88,	929.27,	    762.1,	    637.48],
    fsnames[1]: [1143.73,   1129.83,   1128.39,   1145.44,    1136.32,   1139.05],
    fsnames[2]: [2894.03,    7.85,     8.10,     7.95,     7.88,     7.89],
    fsnames[3]: [1740.76,   1748.96,    1741.77,    1741.17,    1733.39,    1735.85],
    fsnames[4]: [2916.01,	2544.23,	2292.53,	2082.01,	1900.40,	1774.20]
}

all_data = {
    'R/W = 0/10'    : data_0_10,
    'R/W = 3/7'     : data_3_7,
    'R/W = 5/5'     : data_5_5,
    'R/W = 7/3'     : data_7_3,
}

colors = {
    'XFS' : '#bd8066', 
    'NOVA' : '#8b9d4a', # BDD174
    'SPFS' : '#a56cb6', # BD97CE
    'NVLog (AS)' : '#64ae8f', # CCA391
    'NVLog' : '#5FA4D9'
}

markers = {
    'XFS' : '^',
    'NOVA' : 'v',
    'SPFS' : 's',
    'NVLog (AS)' : 'D',
    'NVLog' : 'o',
}

for title in all_data:
    plt.rcParams['axes.xmargin'] = 0
    plt.rcParams['axes.ymargin'] = 0
    plt.rcParams['font.size'] = 10
    plt.rcParams['axes.titlesize'] = 16
    plt.rcParams['axes.labelsize'] = 12
    plt.rcParams['xtick.labelsize'] = 10
    plt.rcParams['ytick.labelsize'] = 10
    plt.rcParams['legend.fontsize'] = 12
    # plt.rcParams['hatch.linewidth'] = 2
    plt.figure(figsize=(4, 2.5))
    plt.xlabel("Sync percentage")
    plt.ylabel("Throughput (MB/s)")
    plt.xticks(ticks=range(0, 6), labels=['0%', '20%', '40%', '60%','80%','100%'])
    plt.ylim(top=3200)
    plt.title('XFS  '+title)
    x = list(range(0, 6))
    for y_title in all_data[title]:
        label = None
        if title == 'R/W = 0/10' and y_title in legends1:
            label = y_title
        if title == 'R/W = 3/7' and y_title in legends2:
            label = y_title
        if title == 'R/W = 0/10' or title == 'R/W = 3/7':
            plt.plot(x, all_data[title][y_title], label=label, 
                    color=colors[y_title], marker=markers[y_title], 
                    markersize=6, markerfacecolor='none')
        else:
            plt.plot(x, all_data[title][y_title], 
                    color=colors[y_title], marker=markers[y_title], 
                    markersize=6, markerfacecolor='none')
    plt.fill(x+list(reversed(x)), all_data[title]['NVLog']+list(reversed(all_data[title]['XFS'])), color='none', edgecolor='#b7e5f2', hatch='/')
    plt.fill(x+list(reversed(x)), all_data[title]['XFS']+[0]*6, color='none', edgecolor='#e1838c', hatch='/')
    # if title == 'R/W = 0/10':
    #     plt.legend(labels=['Ext-4', 'NOVA', 'SPFS'])
    # if title == 'R/W = 3/7':
    #     plt.legend(labels=['P2CACHE (sim)', 'NVPC'])
    if title == 'R/W = 0/10' or title == 'R/W = 3/7':
        plt.legend()
    
    plt.savefig('mixrws_xfs_'+title.replace(' ', '').replace('/','-')+'.pdf', bbox_inches='tight')