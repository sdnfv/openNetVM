"""This script allows a user to create a graph measuring
how the latency and throughput of an NF chain changes based on length"""

from matplotlib import pyplot as plt

### we need to make a 2nd y-axis, so we use the twinx() fnx
ax = plt.subplot()
twin1 = ax.twinx()

### create empty lists to store in future data
x_axis = []

y_axis_latency = []

y_axis_throughput = []

###reads in data from file, adds them to empty variables
file = open("data.txt", "r")

for line in file.readlines():
	linedata = line.split(",")
	x_axis.append(linedata[0])
	y_axis_latency.append(linedata[1])
	y_axis_throughput.append(linedata[2])

#ax.set_ylim(30000, 100000)
#twin1.set_ylim(15000000, 40000000)

### plot the data
for x in y_axis_latency:
	print(x)
p1, = ax.plot(x_axis, y_axis_latency,"-b", label="Latency (ns)")
ax.set_xlabel("NF Chain Length")
ax.set_ylabel("Latency (ns)")

p2, = twin1.plot(x_axis, y_axis_throughput, "-r", label="TX pkts per second")
twin1.set_ylabel("TX pkts per second")

###creates the legend and sets colors
ax.yaxis.label.set_color(p1.get_color())
twin1.yaxis.label.set_color(p2.get_color())

tkw = dict(size=4, width=1.5)
ax.tick_params(axis='y', colors=p1.get_color(), **tkw)
twin1.tick_params(axis='y', colors=p2.get_color(), **tkw)
ax.tick_params(axis='x', **tkw)

ax.legend(handles=[p1, p2])

### saves the .png file in test_performance directory
plt.savefig('performance_graph.png')