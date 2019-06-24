#!/bin/bash

. helper-functions.sh

set -e

sudo rm -rf repository

git clone https://github.com/sdnfv/openNetVM.git repository
check_exit_code "ERROR: Failed installing onvm"

sudo apt-get update
sudo apt-get upgrade -y

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

cd ~

# install dependencies
sudo apt-get install libpcap-dev
sudo apt-get install libreadline-dev

print_header "Installing Lua"
curl -R -O http://www.lua.org/ftp/lua-5.3.5.tar.gz
tar zxf lua-5.3.5.tar.gz
cd lua-5.3.5
sudo make install

print_header "Make pktgen-dpdk"
cd ~/repository/tools/Pktgen/pktgen-dpdk/
make

print_header "Updating lua script"
cp ~/pktgen-config.lua ~/repository/tools/Pktgen/openNetVM-Scripts/

print_header "Pktgen installed"
