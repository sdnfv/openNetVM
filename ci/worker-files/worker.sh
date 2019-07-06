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

sudo apt-get install -y build-essential linux-headers-$(uname -r) git
sudo apt-get install -y libnuma1
sudo apt-get install -y libnuma-dev

cd repository

print_header "Beginning Execution of Workload"

print_header "Installing Environment"
install_env $RUN_PKT
check_exit_code "ERROR: Installing environment failed"

print_header "Building ONVM"
build_onvm
check_exit_code "ERROR: Building ONVM failed"

for mode in $MODES
do
    # run functionality for each mode
    case "$mode" in
    "0")
        . ~/pktgen-worker.sh $PKT_CONFIG
        . ~/speed-worker.sh
        ;;  
    "2")
        . ~/speed-worker.sh
        ;;  
    *)  
        echo "Mode $MODE has not been implemented"
        ;;  
    esac
done

print_header "Performance Tests Completed Successfully"
