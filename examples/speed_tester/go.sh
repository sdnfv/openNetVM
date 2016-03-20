#!/bin/bash

cpu=$1
service=$2
print=$3
dst=$4

if [ -z $service ]
then
        echo "$0 [cpu-list] [Service ID] [PRINT] [DST]"
        echo "$0 3 1 --> core 3, service ID 1"
        echo "$0 3,7,9 1 2 --> cores 3,7, and 9, with Service ID 1, and forwards to service ID 2"
        echo "$0 3,7,9 1 2 1000 --> cores 3,7, and 9, with Service ID 1, forwards to service ID 2,  and Print Rate of 1000"
        exit 1
fi

if [ -z $dst ] && [ -z $print ]
then
        sudo ./speed_tester/$RTE_TARGET/speed_tester -l $cpu -n 3 --proc-type=secondary -- -r $service
elif [ -z $dst ]
then
        sudo ./speed_tester/$RTE_TARGET/speed_tester -l $cpu -n 3 --proc-type=secondary -- -r $service -- -p $print
else
        sudo ./speed_tester/$RTE_TARGET/speed_tester -l $cpu -n 3 --proc-type=secondary -- -r $service -- -d $dst -p $print
fi
