#!/bin/bash

mode=$1
if [ -z $1 ]; then
 mode=0
fi
#mode=0 => Single port connection Flashstack-2 (P2P1=05:00.0) <---> Flashstack-3 (P2P1=05:00.0)
#mode=1 => Two ports connection   Flashstack-2 (P1P1=08:00.0) <---> Flashstack-3 (P1P1=08:00.0) and Flashstack-3(P1P2=08:00.1) <---> Flashstack-4(P1P2=08:00.1) 

#on Flashstack-3
#P1P1 = 08.00.0 ( 10.0.0.3)
#P1P2 = 08.00.1 ( 10.10.0.3)
#P2P1 = 05.00.0 ( 10.0.2.3)
#P2P2 = 05.00.1 ( 10.10.2.3)

#export RTE_SDK=/home/skulk901/dev/openNetVM/dpdk
cur_dir=`cwd`
cd $RTE_SDK/tools
DPDKT=$RTE_SDK/tools

#p1p1 = 08:00.0
#p2p1 = 05:00.0

get_dpdk_status() {
	python $DPDKT/dpdk_nic_bind.py --status
	ifconfig
}

set_to_dpdk_mode0() {
	ifdown p2p1
	python $DPDKT/dpdk_nic_bind.py -b igb_uio 05:00.0
}

reset_dpdk_mode0() {
	python $DPDKT/dpdk_nic_bind.py -u 05:00.0
	ifup p2p1
}

set_to_dpdk_mode1() {
	ifdown p1p1
	python $DPDKT/dpdk_nic_bind.py -b igb_uio 08:00.0
        ifdown p1p2
	python $DPDKT/dpdk_nic_bind.py -b igb_uio 08:00.1
}

reset_dpdk_mode1() {
	python $DPDKT/dpdk_nic_bind.py -u 08:00.0
	ifup p1p1
        python $DPDKT/dpdk_nic_bind.py -u 08:00.1
	ifup p1p2

}

if [ $mode -eq 0 ]; then
	reset_dpdk_mode1
        set_to_dpdk_mode0
else
        reset_dpdk_mode0
	set_to_dpdk_mode1
fi

get_dpdk_status

cd $cur_dir
