#!/bin/bash

cpu=$1
client=$2

if [ -z $client ]
then
        echo "$0 [cpu] [client]"
        exit 1
fi


sudo ./simple_forward/x86_64-native-linuxapp-gcc/forward -c $cpu -n 4 --proc-type=auto -- -n $client

