#!/bin/bash

run_and_fetch_pktgen_stats() {
    python3 ~/run-pktgen.py $WORKER_IP $WORKER_KEY_FILE $WORKER_USER
    # get Pktgen stats from server
    scp -i $WORKER_KEY_FILE -oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null $WORKER_USER@$WORKER_IP:~/repository/tools/Pktgen/pktgen-dpdk/port_stats ~/pktgen_stats
}

log "Running ONVM Manager"
cd ~/repository/onvm
./go.sh 0,1,2,3 3 0xF0 -a 0x7f000000000 -s stdout &>~/onvm_pktgen_stats &
mgr_pid=0
if [ $mgr_pid -ne 0 ] 
then
    echo "ERROR: Starting manager failed"
    return 1
fi

# wait for the manager to come online
sleep 15
log "Manager is live"

log "Running Basic Monitor NF"
cd ~/repository/examples/basic_monitor
./go.sh 1 &>~/bsc_stats &
bsc_mntr_pid=0
if [ $bsc_mntr_pid -ne 0 ] 
then
    echo "ERROR: Starting basic monitor failed"
    return 1
fi

# make sure basic monitor initializes
sleep 10

# run pktgen
log "Collecting Pktgen Statistics"
python3 ~/run-pktgen.py $WORKER_IP $WORKER_KEY_FILE $WORKER_USER
# get Pktgen stats from server
scp -i $WORKER_KEY_FILE -oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null $WORKER_USER@$WORKER_IP:~/repository/tools/Pktgen/pktgen-dpdk/port_stats ~/pktgen_stats

# check if pktgen returned results (non-zero)
if [ -z "$(grep -v "^0$" ~/pktgen_stats | cat)" ]
then
    log "Running Pktgen again"
    python3 ~/run-pktgen.py $WORKER_IP $WORKER_KEY_FILE $WORKER_USER
    # get Pktgen stats from server
    scp -i $WORKER_KEY_FILE -oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null $WORKER_USER@$WORKER_IP:~/repository/tools/Pktgen/pktgen-dpdk/port_stats ~/pktgen_stats
fi

log "Killing Basic Monitor"
sudo pkill -f /basic_monitor

log "Exiting ONVM"
sudo pkill -f onvm_mgr
check_exit_code "ERROR: Killing manager failed"
