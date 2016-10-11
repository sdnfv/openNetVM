#!/bin/bash

cpu=$1
service=$2
print=$3

if [ -z $service ]
then
        echo "$0 [cpu-list] [Service ID] [PRINT]"
        echo "$0 3,4 1 --> cores 3,4 with Service ID of 1"
	echo "$0 3,4 1 10000 --> cores 3,4 with Service ID of 1 and print rate of 10000 packets"
        exit 1
fi

if [ -z $print ]
then
	exec sudo ./flow_table/$RTE_TARGET/flow_table -l $cpu -n 3 --proc-type=secondary -- -r $service
else
	exec sudo ./flow_table/$RTE_TARGET/flow_table -l $cpu -n 3 --proc-type=secondary -- -r $service -- $print
fi
