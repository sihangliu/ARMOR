import numpy as np
import matplotlib.pyplot as plt
import sys
from matplotlib import rc
from sets import Set


xname = "ARMOR"

rc('font',**{'family':'sans-serif','sans-serif':['Times']})
plt.rc('font', family='serif')
plt.figure(figsize=(10,7))

rc('xtick', labelsize=16)
rc('ytick', labelsize=16)


f_8 = open(sys.argv[1], 'r')
f_rand = open(sys.argv[2], 'r')

y_8 = []
y_rand = []
y_re = Set([])

if len(sys.argv) == 4:
    f_re = open(sys.argv[3], 'r')
    for line in f_re:
        y_re.add(int(line))


cnt = 0
eight_rand = 0.0
for line in f_8:
    if line[0].isdigit() == 0:
        continue
    if cnt not in y_re:  
        y_8.append(float(line))
    else:
        y_8.append(0.0)
    eight_rand += float(line)
    cnt += 1

eight_rand /= len(y_8)


cnt = 0
avg_rand = 0.0
for line in f_rand:
    if line[0].isdigit() == 0:
        continue
    y_rand.append(float(line))
    cnt += 1
    avg_rand += float(line)
    #if cnt > 5000:
    #    break

avg_rand /= len(y_rand)

values_8, base_8 = np.histogram(y_8, bins=1000000)
values_rand, base_rand = np.histogram(y_rand, bins=1000000)
cumulative_8 = np.cumsum(values_8) / (len(y_8) + 0.0)
cumulative_rand = np.cumsum(values_rand) / (len(y_rand) + 0.0)

plt_lines = []

plt.hold(True)
plt.xlabel('Relative Error', fontsize=20)
plt.ylabel('CDF', fontsize=20)
plt.title(sys.argv[1])

min_y = min(min(cumulative_8), min(cumulative_rand))

#print np.percentile(y_rand, 99)

axes = plt.gca()
axes.set_ylim([min_y, 1])

l2 = plt.plot(base_rand[:-1], cumulative_rand, '-',  color='r', label="Random", markersize=4, lw=4)
l1 = plt.plot(base_8[:-1], cumulative_8, '-', color = 'b', label=xname, markersize=2, lw=4)
l3 = plt.plot([avg_rand, avg_rand], [min_y, 1], '-.', color='darkred', lw=3, label="Random Avg")
l3 = plt.plot([np.percentile(y_rand, 99), np.percentile(y_rand, 99)], [min_y, 1],  '-.', color='orange', lw=3, label="Random 99th")
l4 = plt.plot([7.97*2, 7.97*2], [min_y, 1], 'k--', lw=3, label="Worst-case Bound")
l4 = plt.plot([eight_rand, eight_rand], [min_y, 1], '--', color='limegreen', lw=3, label=xname+" Avg")
plt.title('Inversek2j', fontsize=20)
plt.xscale('log')
lgd = plt.legend(loc='upper center', numpoints=1, markerscale=1.5, ncol=3, bbox_to_anchor=(0.5, -0.1), fontsize=16)
art = []
art.append(lgd)
plt.savefig('inversek2j_error_cdf.png', additional_artists=art, bbox_inches='tight', dpi=400)
#plt.savefig('inversek2j_error_cdf.pdf', additional_artists=art, bbox_inches='tight')

print "99th", np.percentile(y_rand, 99)
print "rand avg", avg_rand
print "rand_max", max(y_rand)
print "8 avg", eight_rand
print "8 max", max(y_8)
print "rand max", max(y_rand)
