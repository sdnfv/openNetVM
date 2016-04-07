#!/bin/bash

cpu=$1
ports=$2

if [ -z $ports ]
then
        echo "$0 [cpu-list] [port-bitmask]"
        # this works well on our 2x6-core nodes
        echo "$0 0,1,2,6 3 --> cores 0, 1, 2 and 6 with ports 0 and 1"
        echo "Cores will be used as follows in numerical order:"
        echo "  RX thread, TX thread, ..., TX thread for last NF, Stats thread"
        exit 1
fi

sudo rm -rf /mnt/huge/rtemap_*
sudo ./onvm_mgr/onvm_mgr/$RTE_TARGET/onvm_mgr -l $cpu -n 4 --proc-type=primary  -- -p${ports}
