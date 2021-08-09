TEST PERFORMANCE
==
This directory contains a script that generates a graph of how the latency and throughput of an NF chain changes based on its length

Optional Libraries
--
matplotlib library is required for graph generation.
If libpcap is not installed run (might need to update/upgrade):
```
sudo apt-get install python3-matplotlib
```

Compilation and Execution
--
```
cd examples/test_performance
	./test_performanmce.sh -s [specific lengths of NFs]

	OR

	./test_performanmce.sh -i [one int specifying the max # of NFs]

	```

SUMMARY: 

test_performance.sh -- a script that generates data based on a changing # of NFs  

make_data.py -- parses through NF data and outputs it to a data.txt 

make_graph.py -- uses the appropriate data in the data.txt file to make a graph 

clear_files.py -- resets data.txt file so that the script can run again 
