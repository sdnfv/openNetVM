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

. $1 # source config file

if [[ -z $MODES ]]
then
    echo "ERROR: Missing mode argument of config!"
    exit 1
fi

RUN_PKT=false
if [[ $MODES == *0* ]]
then
    # will be running pktgen
    if [[ ! -f $PKT_CONFIG ]]
    then
        echo "ERROR: Mode 0 (Pktgen) must have a config file"
        exit 1
    fi
    RUN_PKT=true 
fi

# source helper functions file
. helper-functions.sh

# sudo apt-get update
# sudo apt-get upgrade -y

# sudo apt-get install -y build-essential linux-headers-$(uname -r) git
# sudo apt-get install -y libnuma1
# sudo apt-get install -y libnuma-dev

# clear lingering stats
rm -rf *stats*

cd repository

print_header "Beginning Execution of Workload"

print_header "Installing Environment"
install_env $RUN_PKT
check_exit_code "ERROR: Installing environment failed"

print_header "Building ONVM"
build_onvm
check_exit_code "ERROR: Building ONVM failed"

print_header "Running ONVM Manager"
cd onvm
./go.sh 0,1,2,3 3 0xF0 -a 0x7f000000000 -s stdout &>~/onvm_stats &
mgr_pid=$?
if [ $mgr_pid -ne 0 ]
then
    echo "ERROR: Starting manager failed"
    exit 1
fi

# wait for the manager to come online
sleep 15
print_header "Manager is live"

for mode in $MODES
do
    # run functionality for each mode
    case "$mode" in
    "0")
        ~/pktgen-worker.sh $PKT_CONFIG
        ;;
    "1")
        ~/speed-worker.sh
        ;;
    *)
        echo "Mode $MODE has not been implemented"
        ;;
    esac
done

print_header "Exiting ONVM"

echo "Manager pid: ${mgr_pid}"
sudo kill $mgr_pid
check_exit_code "ERROR: Killing manager failed"

print_header "Performance Tests Completed Successfully"
