import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import sys
import sys
from matplotlib import rc
rc('font',**{'family':'sans-serif','sans-serif':['Times']})
plt.rc('font', family='serif')

rc('xtick',labelsize=16)
rc('ytick',labelsize=16)

f1 = open(sys.argv[1], 'r')
f2 = open(sys.argv[2], 'r')
y1 = []
y2 = []

fig=plt.gcf()
fig.set_size_inches(10,7)

step = 1

avg_rand = 0.0
cnt = 0
circle_pts = []


for line in f2:
    if (cnt % step == 0):
        num = float(line)
        avg_rand += num
        # find out outliers
        if (num > 0.125):
            circle_pts.append((cnt/step, num))
        y2.append(num)
    cnt+=1

avg_rand /= len(y2)

# no outliers in ARMOR
cnt = 0
for line in f1:
    if (cnt % step == 0):
        num = float(line)
        y1.append(num)
    cnt+=1

#plt.figure(figsize=(10,7))
plt.ylim((0, 0.15))
plt.xlabel('Iteration', fontsize=20)
plt.ylabel('RMSE', fontsize=20)
plt.title('Sobel', fontsize=20)

plt.plot(y1, '|', color='b', markersize=8, label="ARMOR")
plt.plot(y2, '+', color='r', markersize=6, label="Random error")
plt.plot([0, len(y2)], [avg_rand, avg_rand], 'k--', label="Random error avg", markersize=3, lw=2)
plt.plot([0, len(y2)], [np.percentile(y2, 90), np.percentile(y2, 90)], '-', color="blueviolet", label="Random error 90th", markersize=3, lw=2)
plt.text(len(y2)/2, 0.14, 'Outliers', color='r', fontsize=18)
factor = (0.15)/len(y2)*10/7

for x, y in circle_pts:
    plt.gca().add_artist(mpatches.Ellipse((x,y), 50, 50*factor, fill=False, color='r', clip_on=False))

lgd = plt.legend(loc='upper center', bbox_to_anchor=(0.5, -0.1), ncol=2, numpoints=1, markerscale=1.5, fontsize=18)

art=[]
art.append(lgd)

#plt.savefig('sobel_error_64.pdf', bbox_extra_artist=art, bbox_inches='tight')
plt.savefig('sobel_error_64.png', bbox_extra_artist=art, bbox_inches='tight', dpi=400)

print "Avg=",avg_rand
print "ARMORAvg=",np.average(y1)
print "ARMOR90th=",np.percentile(y1,90)
print "Worst=",max(y2)
print "90th=",np.percentile(y2, 90)
print "AvgDiff=", max(y2) - avg_rand
print "90thDiff=", max(y2) - np.percentile(y2, 90)
print "ArmorVar=", np.var(y1)
print "RandVar=", np.var(y2)
print "outlier/avg=", max(y2) / np.average(y2)
print "outlier/99th=", max(y2) / np.percentile(y2, 90)
