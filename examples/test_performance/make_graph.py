#!/usr/bin/python

### This script creates a graph, using data from the "data.txt" file, measuring
### how the latency and throughput of an NF chain changes based on length

from matplotlib import pyplot as plt

x_axis = []

y_axis_latency = []

y_axis_throughput = []

file = open("data.txt", "r")

for line in file.readlines():
	linedata = line.split(",")
	x_axis.append(int(linedata[0]))
	y_axis_latency.append(int(linedata[1]))
	y_axis_throughput.append(int(linedata[2]))


# x_axis.sort()
# y_axis_latency.sort()
# y_axis_throughput.sort()

fig,ax = plt.subplots()
# make a plot
ax.plot(x_axis, y_axis_latency, color="red", marker="o")
# set x-axis label
ax.set_xlabel("NFs",fontsize=14)
# set y-axis label
ax.set_ylabel("Latency (ns)",color="red",fontsize=14)

ax2=ax.twinx()
# make a plot with different y-axis using second axis object
ax2.plot(x_axis, y_axis_throughput,color="blue",marker="o")
ax2.set_ylabel("TX pkts per second",color="blue",fontsize=14)
plt.show()
# save the plot as a file
fig.savefig('graph.png',
            format='png',
            dpi=100,
            bbox_inches='tight')







# from matplotlib import pyplot as plt

# ### we need to make a 2nd y-axis, so we use the twinx() fnx
# ax = plt.subplot()
# twin1 = ax.twinx()

# ### create empty lists to store in future data
# x_axis = []

# y_axis_latency = []

# y_axis_throughput = []

# ###reads in data from file, adds them to empty variables
# file = open("data.txt", "r")

# for line in file.readlines():
# 	linedata = line.split(",")
# 	x_axis.append(int(linedata[0]))
# 	y_axis_latency.append(int(linedata[1]))
# 	y_axis_throughput.append(int(linedata[2]))

# # for datapoint in x_axis:
# # 	print(f"x value: {datapoint}")

# # for datapoint in y_axis_latency:
# # 	print(f"y value: {datapoint}")

# # for datapoint in y_axis_throughput:
# # 	print(f"y value: {datapoint}")

# #ax.set_ylim(30000, 100000)
# #twin1.set_ylim(15000000, 40000000)

# ### plot the data
# p1, = ax.plot(x_axis, y_axis_latency,"-b", label="Latency (ns)")
# ax.set_xlabel("NF Chain Length")
# ax.set_ylabel("Latency (ns)")

# ax.autoscale(enable=False, axis='both', tight=None)

# p2, = twin1.plot(x_axis, y_axis_throughput, "-r", label="TX pkts per second")
# twin1.set_ylabel("TX pkts per second")

# ###creates the legend and sets colors
# ax.yaxis.label.set_color(p1.get_color())
# twin1.yaxis.label.set_color(p2.get_color())

# tkw = dict(size=4, width=1.5)
# ax.tick_params(axis='y', colors=p1.get_color(), **tkw)
# twin1.tick_params(axis='y', colors=p2.get_color(), **tkw)
# ax.tick_params(axis='x', **tkw)

# ax.legend(handles=[p1, p2])



# ### saves the .png file in test_performance directory
# plt.savefig('performance_graph.png')
