#!/bin/bash

log "Running ONVM Manager"
cd ~/repository/onvm || return 1
./go.sh 0,1,2,3 3 0xF0 -a 0x7f000000000 -s stdout &>~/onvm_speed_stats &
mgr_pid=$?
if [ $mgr_pid -ne 0 ]
then
    echo "ERROR: Starting manager failed"
    exit 1
fi

# wait for the manager to come online
sleep 15
log "Manager is live"

log "Running Speed Tester NF"
cd ~/repository/examples/speed_tester || return 1
./go.sh 1 -d 1 &>~/speed_stats &
spd_tstr_pid=$?
if [ $spd_tstr_pid -ne 0 ]
then
    echo "ERROR: Starting speed tester failed"
    return 1
fi

# wait for speed tester to come online
log "Collecting Speed Tester Statistics"
sleep 15

log "Killing Speed Tester"
sudo pkill -f /speed_tester

log "Exiting Manager"

echo "Manager pid: ${mgr_pid}"
sudo pkill -f onvm_mgr
check_exit_code "ERROR: Killing manager failed"
