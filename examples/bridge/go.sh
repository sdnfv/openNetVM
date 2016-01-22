#!/bin/bash

cpu=$1
client=$2

if [ -z $client ]
then
        echo "$0 [cpu-list] [ID]"
        echo "$0 3 0 --> core 3 ID 0"
        echo "$0 3,7,9 1 --> cores 3,7, and 9 with ID 1"
        exit 1
fi


sudo ./bridge/x86_64-native-linuxapp-gcc/bridge -l $cpu -n 4 --proc-type=auto -- -n $client -- #-p 1000000000
