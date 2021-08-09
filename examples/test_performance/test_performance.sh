#!/bin/bash

echo "Type of script (iterative (i) or selective (s) ) = $1" 

if [[ $1 = "-s" ]]
then
	echo "Running selective version"
	NUM_NFS_LIST=( "$@" )

	# check that inputs are valid (positive) integers

	VALID_INPUT=true

	for (( i=1; i<${#NUM_NFS_LIST[@]}; i++));
	do
		if [[ ${NUM_NFS_LIST[$i]} =~ ^[+]?[0-9]+$ ]];
		then
			:
		else 
			VALID_INPUT=false
		fi
	done

	if [[ "$VALID_INPUT" != true ]];
	then
		echo "Error: your inputs (desired chain lengths) must be positive integers"
		exit 0
	fi

	# run manager at each chain length
	for (( i=1; i<${#NUM_NFS_LIST[@]}; i++));
	do 
		echo "Running ${NUM_NFS_LIST[$i]} NFs"
		python3 change_nfs.py ${NUM_NFS_LIST[$i]} # adds an additional simple_forward NF for the next loop iteration

		echo "Running onvm mgr"
		cd ../../
		./onvm/go.sh -k 1 -n 0xFFFFF -s stdout -t 30 &
		
		sleep 15s

		echo "Running ${NUM_NFS_LIST[$i]} NFs"
		cd examples
		python3 run_group.py example_nf_deploy_test_p_template.json # starts running the set NFs for 30sec, enough time to collect ample data

		wait

		cd test_performance
		echo "Making data"
		python3 make_data.py ${NUM_NFS_LIST[$i]} # adds the data and averages it

	done

	python3 ./make_graph.py # makes the graph

	python3 ./clear_files.py # resets data files and nf_deploy json files

elif [[ $1 = "-i" ]]
then
	echo "Running iterative version"
	NUM_NFS=2

	# check if the input is valid
	VALID_INPUT=true

	if [[ $2 =~ ^[+]?[0-9]+$ ]];
	then
		:
	else 
		VALID_INPUT=false
	fi

	if [[ "$VALID_INPUT" != true ]];
	then
		echo "Error: your input (max NFs) must be a positive integer"
		exit 0
	fi

	# iterate through each version of the NF
	while [ $NUM_NFS -le $2 ]
	do 
		echo "Running $NUM_NFS NFs"
		echo "Running onvm mgr"
		cd ../../
		./onvm/go.sh -k 1 -n 0xFFFFF -s stdout -t 30 &
		
		sleep 15s

		echo "Running $NUM_NFS NFs"
		cd examples
		python3 ./run_group.py ./example_nf_deploy_test_p_template.json # starts running the set NFs for 30sec, enough time to collect ample data

		sleep 30s

		cd test_performance
		echo "Making data"
		python3 ./make_data.py $NUM_NFS # adds the data and averages it

		NUM_NFS=$((NUM_NFS+1))

		echo "Changing NFs"
		python3 ./change_nfs.py $NUM_NFS # adds an additional simple_forward NF for the next loop iteration
	done

	python3 ./make_graph.py # makes the graph

	python3 ./clear_files.py # resets data files
else
	echo "Error: incorrect 1st input; must be either \"-i\" or \"-s\""
fi
