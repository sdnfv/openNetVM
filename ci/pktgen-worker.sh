#!/bin/bash

cd ~
. helper-functions.sh

if [[ -z $1 ]]
then
    echo "ERROR: Missing first argument, path to pktgen config file"
    exit 1
fi

if [[ ! -f $1 ]]
then
    echo "ERROR: Could not find config file at given path!"
    exit 1
fi

. $1 # source the variables from pktgen config file

print_header "Running Basic Monitor NF"
cd repository/examples/basic_monitor
./go.sh 1 &>~/bsc_stats &
bsc_mntr_pid=$?
if [ $bsc_mntr_pid -ne 0 ]
then
    echo "ERROR: Starting basic monitor failed"
    return 1
fi

# run pktgen
print_header "Collecting Pktgen Statistics"
python3 ~/run-pktgen.py $PKT_WORKER_IP $PKT_WORKER_KEY_FILE
# get Pktgen stats from server
scp -i $PKT_WORKER_KEY_FILE -oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null $PKT_WORKER_IP:~/repository/tools/Pktgen/pktgen-dpdk/port_stats ~/port_stats

print_header "Killing Basic Monitor"
sudo pkill -f /basic_monitor
