import matplotlib.pyplot as plt
import numpy as np

data_usage = [0, 0.753623962, 1.550544739, 2.349071503, 3.145370483, 3.945049286, 4.693443298, 5.487110138, 6.282615662, 7.077384949, 7.87915802, 8.13551712, 8.410934448, 8.78237915, 9.491573334, 10.28536987, 11.08129501, 11.87234879, 12.6654892, 13.20856476, 13.56175232, 14.29384232, 15.09295273, 15.88820267, 16.68635941, 17.48386765, 17.94021225, 18.38407516, 19.1717453, 19.96328354, 20.7511673, 21.54438782, 22.27402496, 22.65717316, 23.28288651, 24.07394791, 24.8605423, 25.64626312, 26.43754578, 26.95785141, 27.3092804, 28.04926682, 28.8398056, 29.63798141, 30.43188477, 31.22434616, 31.69117355, 32.10989761, 32.90020752, 33.69409943, 34.48475647, 35.27923965, 36.02380371, 36.39281082, 37.00072861, 37.79504776, 38.59202576, 39.38690948, 40.1823616, 40.77472687, 41.12265015, 41.8367157, 42.63382339, 43.42645645, 44.22589874, 45.01856995, 45.53789902, 45.92774582, 46.72298813, 47.51740265, 48.31286621, 49.10745239, 49.86828995, 50.24634171, 50.82147598, 51.61396408, 52.40619278, 53.20307541, 53.99673462, 54.57976913, 54.91495132, 55.62613297, 56.42541504, 57.2180748, 58.00705719, 58.7983551, 59.21194077, 59.71521759, 60.51019287, 61.30299759, 62.09748459, 62.88770676, 63.581707, 63.9455452, 64.60352707, 65.3992691, 66.19955063, 66.99728012, 67.79410934, 68.32579803, 68.63830185, 69.433918, 70.23526382, 71.03094864, 71.82666779, 72.62968826, 73.08012009, 73.5086937, 74.30586624, 75.09907532, 75.89056396, 76.6814003, 77.47143936, 77.84588242, 78.347332, 79.13648987, 79.92823029, 80.72133255, 81.51211166, 82.15065765, 82.50216293, 82.53497314, 82.53497314, 82.53497314, 82.53497314, 82.53497314, 82.5396843, 82.5396843, 82.5396843, 82.5396843, 82.5396843, 82.5396843, 82.5396843, 82.5396843, 82.5396843, 82.5396843, 82.5396843, 82.5396843, 82.5396843, 82.5396843, 82.5396843, 82.5396843, 82.5396843, 82.5396843, 82.5396843, 82.5396843, 82.5396843, 82.5396843, 82.5396843, 82.5396843]
data_thp = [794.5546875, 796.8828125, 798.2929688, 796.265625, 799.4179688, 746.9492188, 793.4882813, 795.2910156, 794.6875, 801.4335938, 249.1601563, 219.75, 329.3085938, 670.9882813, 793.8554688, 796.2929688, 791.0371094, 793.0546875, 560.828125, 338.59375, 647.1210938, 798.7109375, 795.546875, 797.8085938, 797.296875, 494.5058594, 343.5039063, 775.5625, 788.2890625, 786.828125, 793.7851563, 784.03125, 374.171875, 503.859375, 790.7548828, 786.7109375, 789.453125, 786.3945313, 622.8320313, 333.1289063, 577.15625, 791.5429688, 796.328125, 793.5, 795.7929688, 581.7382813, 330.6484375, 686.125, 789.7851563, 789.7070313, 794.6328125, 790.6601563, 455.5019531, 397.5078125, 794.3710938, 796.5664063, 794.046875, 799.6015625, 747.9726563, 359.859375, 467.5625, 794.5634766, 792.1328125, 798.5390625, 794.421875, 714.6640625, 333.0585938, 575.0390625, 795.375, 796.1992188, 796.2685547, 793.6210938, 546.4296875, 334.4023438, 752.09375, 796.6367188, 791.5429688, 797.0351563, 792.90625, 377.1816406, 383.234375, 792.2421875, 794.3554688, 790.78125, 793.703125, 675.5703125, 335.1953125, 620.3662109, 795.4179688, 792.1601563, 791.1757813, 794.2421875, 539.734375, 328.90625, 770.3007813, 795.828125, 796.8505859, 801.2070313, 794.9921875, 397.828125, 369.3867188, 796.1289063, 800.6289063, 794.6914063, 796.8476563, 790.3085938, 346.4121094, 464.2539063, 793.5390625, 796.5976563, 789.9257813, 792.0351563, 743.125, 325.5117188, 516.7705078, 789.3554688, 791.0195313, 793.1679688, 790.796875, 594.2851563, 309.8007813, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]

plt.rcParams['axes.xmargin'] = 0
plt.rcParams['axes.ymargin'] = 0
# plt.figure(figsize=(6, 3))
fig, axes = plt.subplots(2, 1, sharex=True, figsize=(6, 3))
# plt.subplots_adjust(wspace=0, hspace=0)
# plt.ylabel("NVM Usage (GB)")
plt.xlabel("Time (sec)")
# plt.ylim(top=20)
# plt.xticks(np.arange(6)*4+1, clusters)
# plt.ticklabel_format(axis='y', style='sci', scilimits=(0,0))
# plt.title(d)
# for i, clust in enumerate(clusters):
#     h = plt.bar(np.arange(3)+4*i, data[d][clust], color=colors, hatch=hatches, label=fsnames)

# plt.legend(h, fsnames)
# plt.plot(data)
axes[0].set_ylabel("NVM Usage (GB)")
axes[0].set_ylim(top=100)
axes[1].set_ylabel("Throughput (MB/s)")
axes[1].set_ylim(top=1000)
axes[0].plot(data_usage)
axes[1].plot(data_thp)

plt.savefig('gc-nocompact.pdf', bbox_inches='tight')