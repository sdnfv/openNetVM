#!/bin/bash

cpu=$1
ports=$2

if [ -z $ports ]
then
        echo "$0 [cpu-list] [port-bitmask]"
        # this works well on our 2x6-core nodes
        echo "$0 0,1,2,6 3 --> cores 0, 1, and 9 with ports 0 and 1"
        exit 1
fi

sudo rm -rf /mnt/huge/rtemap_*
sudo ./onvm_mgr/onvm_mgr/x86_64-native-linuxapp-gcc/onvm_mgr -l $cpu -n 3 --proc-type=primary  -- -p${ports}
