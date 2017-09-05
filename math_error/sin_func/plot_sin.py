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

step = 100
cnt = 0

for line in f:
    floats = map(float, line.split())
    if cnt % step == 0:
        x.append(floats[0])
        y.append(floats[1])
    if floats[1] > max_err:
        max_err = floats[1]
    cnt += 1

axes = plt.gca()
if sys.argv[1][0] == 'a':
    ylimit = 0.0001
else:
    ylimit = 0.006
axes.set_ylim([0, ylimit])
#plt.ticklabel_format(style='sci', axis='y', scilimits=(0,0))
plt.xlabel(r'Input $x$', fontsize=24)
plt.ylabel(r'Relative Error of $\sin(x)$', fontsize=24)
#plt.title(sys.argv[2],fontsize=20)
plt.plot(x, y, 'b+', markersize=5) #, label="Error in " + sys.argv[1] + " Function")
#plt.plot([min(x), max(x)], [np.percentile(y,99), np.percentile(y,99)], label="99th Percentile = " + "{:.2e}".format(np.percentile(y,99)))
fig = plt.gcf()
fig.set_size_inches(7, 5)
plt.xlim(x[0], x[-1])
plt.xticks([np.pi, 3.5,4, 4.5, 5, 5.5, 6, 2*np.pi], 
        [r'$\pi$',r'$3.5$',r'$4$',r'$4.5$',r'$5$',r'$5.5$',r'$6$',r'$2\pi$'])


#plt.text(3.5, 0.001, r'$\pi$', fontsize = 20)
#plt.text(6, 0.001,  r'$2\pi$', fontsize = 20)


#plt.yscale('log')

#lgd = plt.legend(loc='upper center', numpoints=1, markerscale=1.5, ncol=2, bbox_to_anchor=(0.5, -0.15), fontsize=18)
art = []
#art.append(lgd)


plt.savefig('sin.pdf', additional_artists=art, bbox_inches='tight')
#x.sort()
#print np.percentile(y,99)
