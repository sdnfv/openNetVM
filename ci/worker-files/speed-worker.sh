#!/bin/bash

. ~/helper-functions.sh

print_header "Running Speed Tester NF"
cd ~/repository/examples/speed_tester
./go.sh 1 -d 1 &>~/speed_stats &
spd_tstr_pid=$?
if [ $spd_tstr_pid -ne 0 ]
then
    echo "ERROR: Starting speed tester failed"
    # exit 1
fi

# wait for speed tester to come online
print_header "Collecting Speed Tester Statistics"
sleep 15

print_header "Killing Speed Tester"
sudo pkill -f /speed_tester
