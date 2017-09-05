import numpy as np
import matplotlib.pyplot as plt
import sys
from matplotlib import rc
rc('font',**{'family':'sans-serif','sans-serif':['Times']})
plt.rc('font', family='serif')
rc('xtick', labelsize=20)
rc('ytick', labelsize=20)

fig=plt.gcf()
fig.set_size_inches(10,10)

f = open(sys.argv[1])
x = []
y = []

max_err = 0.0

step = 5
cnt = 0

for line in f:
    y.append(float(line))

axes = plt.gca()
axes.set_ylim([0, 0.1])

#plt.ticklabel_format(style='sci', axis='y', scilimits=(0,0))
plt.xlabel('Input x', fontsize=24)
plt.ylabel(r'Relative Error of $a-b$', fontsize=24)
#plt.title(r'$a = (1 + e^{-x}), b = 1$',fontsize=22)
plt.plot(range(1,len(y)+1), y, 'b', marker='s', markersize=10)
fig = plt.gcf()
fig.set_size_inches(7, 5)


plt.savefig('sub.pdf', bbox_inches='tight')
