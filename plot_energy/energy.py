import numpy as np
import matplotlib.pyplot as plt
import matplotlib
from matplotlib import rc
import sys


labels = ['Black-Scholes', 'FFT', 'Inversek2j', 'Jmeint', 'K-means', 'Sobel']
sizes = ['8Gb', '16Gb', '32Gb', '64Gb']
colors=['grey', 'darkgrey', 'lightgrey', 'white']

rc('xtick', labelsize=28)
rc('ytick', labelsize=24)
rc('font',**{'family':'sans-serif','sans-serif':['Times New Romans'], 'weight':'bold'})
plt.rc('font', family='serif')


f1 = open('checker.txt')
f2 = open('reshuffle.txt')
f3 = open('dram.txt')
f4 = open('core.txt')

name=['Reshuffle + Approx-bit Cache', 'DRAM', 'Core + Checker']


r1 = {'8Gb':[], '16Gb':[], '32Gb':[], '64Gb':[]}
r2 = {'8Gb':[], '16Gb':[], '32Gb':[], '64Gb':[]}
r3 = {'8Gb':[], '16Gb':[], '32Gb':[], '64Gb':[]}
r4 = {'8Gb':[], '16Gb':[], '32Gb':[], '64Gb':[]}

for line in f1:
    floats = [float(x) for x in line.split()]
    
    r1['8Gb'].append(floats[0])
    r1['16Gb'].append(floats[1])
    r1['32Gb'].append(floats[2])
    r1['64Gb'].append(floats[3])

for line in f2:
    floats = [float(x) for x in line.split()]
    
    r2['8Gb'].append(floats[0])
    r2['16Gb'].append(floats[1])
    r2['32Gb'].append(floats[2])
    r2['64Gb'].append(floats[3])

for line in f3:
    floats = [float(x) for x in line.split()]
        
    r3['8Gb'].append(floats[0])
    r3['16Gb'].append(floats[1])
    r3['32Gb'].append(floats[2])
    r3['64Gb'].append(floats[3])

i = 0
for line in f4:
    floats = [float(x) for x in line.split()]
        
    r4['8Gb'].append(floats[0] + r1['8Gb'][i])
    r4['16Gb'].append(floats[1] + r1['16Gb'][i])
    r4['32Gb'].append(floats[2] + r1['32Gb'][i])
    r4['64Gb'].append(floats[3] + r1['64Gb'][i])
    
    i += 1

x = range(1,len(labels)+1)
total_width = 0.8
width = total_width / len(sizes)
x = [x[i] - total_width / 2 for i in range(len(labels))]

curr_size = 0
for size in sizes:
    if curr_size == len(sizes)/2:    
        curr_x = [x[j] + curr_size * width for j in range(len(labels))]
        
        plt.bar(curr_x, r4[size], label=name[2], width=width, color=colors[0], tick_label=labels)
        plt.bar(curr_x, r3[size], bottom=r4[size], label=name[1], width=width, color=colors[1], tick_label=labels)
        plt.bar(curr_x, r2[size], bottom=[r3[size][i] + r4[size][i] for i in range(0,len(r3[size]))], label=name[0], width=width, color=colors[2], tick_label=labels)
        #plt.bar(curr_x, r1[size], bottom=[r2[size][i] + r3[size][i] + r4[size][i] for i in range(0,len(r3[size]))], label=name[0], width=width,  color=colors[3], tick_label=labels)
    else:
        curr_x = [x[j] + curr_size * width for j in range(len(labels))]
        
        plt.bar(curr_x, r4[size], label='_nolegend_', width=width, color=colors[0])
        plt.bar(curr_x, r3[size], bottom=r4[size], label='_nolegend_', width=width, color=colors[1])
        plt.bar(curr_x, r2[size], bottom=[r3[size][i] + r4[size][i] for i in range(0,len(r3[size]))], label='_nolegend_', width=width, color=colors[2])
        #plt.bar(curr_x, r1[size], bottom=[r2[size][i] + r3[size][i] + r4[size][i] for i in range(0,len(r3[size]))], label=name[0], width=width,  color=colors[3])


    curr_size += 1


#plt.text(1.6, 1.02, '8Gb', fontsize=12)
#plt.text(1.8, 1.02, '16Gb', fontsize=12)
#plt.text(2, 1.02, '32Gb', fontsize=12)
#plt.text(2.2, 1.02, '64Gb', fontsize=12)

plt.annotate('8Gb', xy=(1.7, 1.005), xytext=(1.45, 1.03), arrowprops=dict(arrowstyle='-|>', facecolor='black'), fontsize=18)
plt.annotate('16Gb', xy=(1.9, 1.002), xytext=(1.7, 1.025), arrowprops=dict(arrowstyle='-|>', facecolor='black'), fontsize=18)
plt.annotate('32Gb', xy=(2.1, 0.997), xytext=(2.03, 1.02), arrowprops=dict(arrowstyle='-|>', facecolor='black'), fontsize=18)
plt.annotate('64Gb', xy=(2.3, 0.99), xytext=(2.37, 1.012), arrowprops=dict(arrowstyle='-|>', facecolor='black'), fontsize=18)


lgd = plt.legend(loc='center', bbox_to_anchor=(0.5, 1.10), ncol=3, fontsize=24)
lgd.get_frame().set_linewidth(0.0)
art=[]
art.append(lgd)

plt.ylabel('Normalized Energy', fontsize=28, weight='bold')
#plt.yticks([0.75,0.85,0.95,1.05])

axes = plt.gca()
axes.set_axisbelow(True)
axes.yaxis.grid(color='grey', linewidth=1, linestyle='-')
axes.set_ylim([0.75, 1.06])
axes.set_xlim([0.4, 6.6])
axes.spines['right'].set_visible(False)
axes.spines['left'].set_visible(True)
axes.spines['top'].set_visible(False)
axes.yaxis.set_ticks_position('left')
axes.xaxis.set_ticks_position('bottom')

for axis in ['top','bottom','left','right']:
      axes.spines[axis].set_linewidth(2)

fig = matplotlib.pyplot.gcf()
fig.set_size_inches(20, 4.5)
fig.savefig('energy.pdf', additional_artists=art, bbox_inches='tight')
