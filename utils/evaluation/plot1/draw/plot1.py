import matplotlib.pyplot as plt

fsnames = ['Ext-4', 'NOVA', 'SPFS', 'P2CACHE (sim)', 'NVPC']
legends1 = ['Ext-4', 'NOVA', 'SPFS']
legends2 = ['P2CACHE (sim)', 'NVPC']

# 生成随机数据
data_0_10 = {
    fsnames[0]: [3061.46,   871.62,     516.06,     367.95,     284.6,      231.34],
    fsnames[1]: [813.98,    820.09,    824.39,    803.81,     822.14,    811.25 ],
    fsnames[2]: [2440.10,   338.98,     330.82,     323.35,     319.91,     312.37],
    fsnames[3]: [1031.24,   1036.43,    1030.54,    1031.92,    1029.66,    1035.40],
    fsnames[4]: [3025.14,   2150.78,    1686.06,    1391.01,    1187.55,    1027.10]
}

data_3_7 = {
    fsnames[0]: [3011.79,   1099.12,    677.77,     491.30,     384.84,     316.83],
    fsnames[1]: [888.26,    897.36,    896.16,    917.32,     906.07,    904.95 ],
    fsnames[2]: [2509.80,   347.08,     342.40,     337.43,     331.62,     326.60],
    fsnames[3]: [1273.89,   1280.54,    1280.01,    1282.72,    1270.75,    1276.05],
    fsnames[4]: [3000.12,   2345.12,    1921.25,    1646.37,    1442.29,    1282.67]
}

data_5_5 = {
    fsnames[0]: [3049.45,   1349.44,    871.49,     642.63,     512.00,     423.61],
    fsnames[1]: [1010.24,   973.25,    1006.11,   1001.85,    1000.38,   987.14 ],
    fsnames[2]: [2633.52,   355.61,     348.30,     346.00,     340.94,     337.78],
    fsnames[3]: [1542.56,   1538.7,     1534.47,    1535.61,    1542.55,    1544.49],
    fsnames[4]: [3029.60,   2538.93,    2179.50,    1914.04,    1701.54,    1533.75]
}

data_7_3 = {
    fsnames[0]: [3112.49,   1746.44,    1225.37,    946.54,     769.25,     651.55],
    fsnames[1]: [1143.73,   1129.83,   1128.39,   1145.44,    1136.32,   1139.05],
    fsnames[2]: [2817.1,    359.74,     356.40,     353.67,     350.20,     347.09],
    fsnames[3]: [1940.04,   1936.98,    1938.80,    1936.42,    1942.47,    1941.24],
    fsnames[4]: [3104.62,   2746.19,    2497.68,    2263.01,    2085.58,    1939.40]
}

all_data = {
    'R/W = 0/10'    : data_0_10,
    'R/W = 3/7'     : data_3_7,
    'R/W = 5/5'     : data_5_5,
    'R/W = 7/3'     : data_7_3,
}

colors = {
    'Ext-4' : '#bd8066', 
    'NOVA' : '#8b9d4a', # BDD174
    'SPFS' : '#a56cb6', # BD97CE
    'P2CACHE (sim)' : '#64ae8f', # CCA391
    'NVPC' : '#5FA4D9'
}

markers = {
    'Ext-4' : '^',
    'NOVA' : 'v',
    'SPFS' : 's',
    'P2CACHE (sim)' : 'D',
    'NVPC' : 'o',
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
    plt.figure(figsize=(4, 3))
    plt.xlabel("Sync percentage")
    plt.ylabel("Throughput (MB/s)")
    plt.xticks(ticks=range(0, 6), labels=['0%', '20%', '40%', '60%','80%','100%'])
    plt.ylim(top=3200)
    plt.title(title)
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
    plt.fill(x+list(reversed(x)), all_data[title]['NVPC']+list(reversed(all_data[title]['Ext-4'])), color='none', edgecolor='#b7e5f2', hatch='/')
    plt.fill(x+list(reversed(x)), all_data[title]['Ext-4']+[0]*6, color='none', edgecolor='#e1838c', hatch='/')
    # if title == 'R/W = 0/10':
    #     plt.legend(labels=['Ext-4', 'NOVA', 'SPFS'])
    # if title == 'R/W = 3/7':
    #     plt.legend(labels=['P2CACHE (sim)', 'NVPC'])
    if title == 'R/W = 0/10' or title == 'R/W = 3/7':
        plt.legend()
    
    plt.savefig('plot1_'+title.replace(' ', '').replace('/','-')+'.pdf', bbox_inches='tight')