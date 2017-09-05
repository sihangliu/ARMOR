import numpy as np
import matplotlib.pyplot as plt
import sys
from matplotlib import rc


xname = "ARMOR"

rc('font',**{'family':'sans-serif','sans-serif':['Times']})
plt.rc('font', family='serif')
#plt.figure(figsize=(10,7))
rc('xtick', labelsize=16)
rc('ytick', labelsize=16)

fig=plt.gcf()
fig.set_size_inches(10,7)

f_8 = open(sys.argv[1], 'r')
f_rand = open(sys.argv[2], 'r')

y_8 = []
y_rand = []

cnt = 0
eight_rand = 0.0
for line in f_8:
    y_8.append(float(line))
    cnt += 1
    eight_rand += float(line)

eight_rand /= len(y_8)


cnt = 0
avg_rand = 0.0
for line in f_rand:
    y_rand.append(float(line))
    cnt += 1
    avg_rand += float(line)

avg_rand /= len(y_rand)

plt_lines = []

plt.hold(True)
plt.xlabel('#Input', fontsize=20)
plt.ylabel('Relative Error', fontsize=20)
plt.title(sys.argv[1])
#print np.percentile(y_rand, 99)
axes = plt.gca()
axes.set_xlim([0, len(y_rand)])
plt.plot(y_rand, '+',  color='r', label="Random", markersize=5)
plt.plot(y_8, '.', color = 'b', label=xname, markersize=3)
plt.plot([0, len(y_rand)], [avg_rand, avg_rand], '-', color='darkred', lw=2, label="Random Avg")
plt.plot([0, len(y_rand)], [np.percentile(y_rand, 99), np.percentile(y_rand, 99)], '-', color='orange', lw=2, label="Random 99th")
plt.plot([0, len(y_rand)], [0.249, 0.249], 'k-', lw=2, label="Worst-case Bound")
plt.plot([0, len(y_rand)], [eight_rand, eight_rand], 'g-', lw=2, label=xname + " Avg")
plt.title('Black-Scholes', fontsize=20)
plt.yscale('log')
lgd = plt.legend(loc='upper center', numpoints=1, markerscale=1.5, ncol=3, bbox_to_anchor=(0.5, -0.1), fontsize=16)
art = []
art.append(lgd)
plt.savefig('blackscholes_error.png', additional_artists=art, bbox_inches='tight', dpi=400)
#plt.savefig('blackscholes_error.pdf', additional_artists=art, bbox_inches='tight')

#print "99th", np.percentile(y_rand, 99)
#print "rand avg", avg_rand
#print "8 avg", eight_rand
#print "8 max", max(y_8)
#print "rand max", max(y_rand)
