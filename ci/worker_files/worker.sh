#!/bin/bash

if [[ -z $1 ]]
then
    echo "ERROR: Missing first argument, path to config file!"
    exit 1
fi

if [[ ! -f $1 ]]
then
    echo "ERROR: Could not find config file at given path!"
    exit 1
fi

. "$1" # source config file

if [[ -z $WORKER_MODE ]]
then
    echo "ERROR: Missing mode argument of config!"
    exit 1
fi

# source helper functions file
. helper-worker-functions.sh

sudo apt-get install -y build-essential linux-headers-"$(uname -r)" git
sudo apt-get install -y libnuma1
sudo apt-get install -y libnuma-dev
sudo apt-get install -y python3

cd repository || echo "ERROR: couldn't cd into repository" && exit 1
log "Beginning Execution of Workload"

log "Installing Environment"
install_env
check_exit_code "ERROR: Installing environment failed"

log "Building ONVM"
build_onvm
check_exit_code "ERROR: Building ONVM failed"

for mode in $WORKER_MODE
do
    # run functionality for each mode
    case "$mode" in
    "0")
        . ~/speed-tester-worker.sh
        . ~/pktgen-worker.sh
        . ~/mtcp-worker.sh
        ;;
    "1")
        . ~/speed-tester-worker.sh
        ;;
    "2")
        . ~/pktgen-worker.sh
        ;;
    "3")
        . ~/mtcp-worker.sh
        ;;
    *)
        echo "Worker mode $mode has not been implemented"
        ;;
    esac
done

log "Performance Tests Completed Successfully"
