#!/bin/bash

#assume 5 NFs

NUM_NFS=2

MAX_NFS=6 #tbd

while [ $NUM_NFS != $MAX_NFS ]
do
	echo "Running onvm mgr"
	cd ../../
	./onvm/go.sh -k 1 -n 0xF8 -s stdout -t 30 &
	

	sleep 15s

	echo "Running $NUM_NFS NFs"
	cd examples
	python3 ./run_group.py example_nf_deploy.json #starts running the set NFs for 30sec, enough time to collect ample data

	sleep 30s

	cd test_performance
	echo "Making data"
	python3 ./make_data.py <<< $NUM_NFS #adds the data and averages it

	NUM_NFS=$((NUM_NFS+1))

	echo "Changing NFs"
	python3 ./change_nfs.py #adds an additional simple_forward NF for the next loop iteration
done

python3 ./make_graph.py # makes the graph

python3 ./clear_files.py #resets data files