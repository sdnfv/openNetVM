#This script creates the data necessary to make a graph of how
#the latency and throughput of an NF chain changes based on length
#outputs 3 numbers to 'data.txt': [#nfs, TX average, and latency average]

num_nfs = input("Enter the number of nfs: ")

file1 = open("/users/elliott1/openNetVM/examples/output_files/log-speed_tester-1.txt", "r")


TX_data = []
latency_data = []

### adds the correct values from the log-speed_tester file
for line in file1.readlines():
	if ('TX pkts per second:  ' in line):
		index = line.index(':  ') + 3 #spacing is weird in the data file
		TX_data.append(line[index:])
	if ('Avg latency nanoseconds: ' in line):
		index = line.index(': ') + 2
		latency_data.append(line[index:])

file1.close()

### find the averages of the data

TX_average = 0

for x in TX_data:
	TX_average+=int(x)

TX_average /= len(TX_data)

latency_average = 0

for x in latency_data:
	latency_average+=int(x)
latency_average /= len(latency_data)

##writes the data out as 3 values, all seperated by commas, to the data.txt file

f = open("data.txt", "a")
f.write(num_nfs)
f.write(',')
f.write(str(int(TX_average)))
f.write(',')
f.write(str(int(latency_average)))
f.write(',\n')