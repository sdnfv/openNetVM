#!/bin/bash

 # source helper functions file
. helper-functions.sh

sudo apt-get update
sudo apt-get upgrade -y

sudo apt-get install -y build-essential linux-headers-$(uname -r) git
sudo apt-get install -y libnuma1
sudo apt-get install -y libnuma-dev

cd repository

print_header "Beginning Execution of Workload"

print_header "Installing Environment"
install_env
check_exit_code "ERROR: Installing environment failed"

print_header "Building ONVM"
build_onvm
check_exit_code "ERROR: Building ONVM failed"

print_header "Running ONVM Manager"
cd onvm
./go.sh 0,1,2,3 0 0xF0 -s web -a 0x7f000000000 &
mgr_pid=$?
if [ $mgr_pid -ne 0 ]
then
    echo "ERROR: Starting manager failed"
    exit 1
fi

# wait for the manager to come online
sleep 15

# Some weirdness with PIDS, look into this
# Works, but we don't know why

print_header "Running Speed Tester NF"
cd ../examples/speed_tester
./go.sh 1 -d 1 &>~/stats &
spd_tstr_pid=$?
if [ $spd_tstr_pid -ne 0 ]
then
    echo "ERROR: Starting speed tester failed"
    sudo kill $mgr_pid
    exit 1
fi

 # wait for speed tester to come online
print_header "Collecting Statistics"
sleep 15

print_header "Exiting ONVM"

echo "Speed tester pid: ${spd_tstr_pid}"
echo $spd_tstr_pid
echo "Manager pid: ${mgr_pid}"
echo $mgr_pid

sudo kill $spd_tstr_pid
check_exit_code "Error: Killing speed tester failed!"

sudo kill $mgr_pid
check_exit_code "ERROR: Killing manager failed"

print_header "Performance Tests Completed Successfully"
