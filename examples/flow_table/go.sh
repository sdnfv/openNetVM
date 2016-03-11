#!/bin/bash

cpu=$1
client=$2
app=$3

if [ -z $2 ]
then
        echo "$0 cpu ID (APP-FLAGS)"
        echo "$0 30 0 --> cores 4,5 ID 0"¬
        exit 1
fi

#  make ONVM=~/openNetVM/onvm/ USER_FLAGS="-g -O0 -DDEBUG_PRINT="

sudo ./build/flow_table -c $cpu -n 4 --proc-type=auto -- -r $client -- $app
#sudo ./flow_table/x86_64-native-linuxapp-gcc/flow_table -c $cpu -n 4 --proc-type=auto -- -n $client

# sudo gdb ./build/flow_table -ex "run -c 10 -n 4 --proc-type=secondary -- -n 0"^C
