#!/bin/bash

. helper-install-functions.sh

set -e

sudo rm -rf repository

git clone https://github.com/sdnfv/openNetVM.git repository
check_exit_code "ERROR: Failed cloning"

print_header "Installing Dependencies"
sudo apt-get update
sudo apt-get upgrade -y
sudo apt-get install -y build-essential linux-headers-"$(uname -r)" git
sudo apt-get install -y libnuma1
sudo apt-get install -y libnuma-dev
sudo apt-get install libpcap-dev
sudo apt-get install libreadline-dev

print_header "Installing Lua"
curl -R -O http://www.lua.org/ftp/lua-5.3.5.tar.gz
tar zxf lua-5.3.5.tar.gz
cd lua-5.3.5
sudo make linux test
sudo make install

cd repository

print_header "Installing Environment"
install_env "$RUN_PKT"
check_exit_code "ERROR: Installing environment failed"

print_header "Make pktgen-dpdk"
cd ~/repository/tools/Pktgen/pktgen-dpdk/
make

print_header "Updating lua script"
cp ~/pktgen-timed-config.lua ~/repository/tools/Pktgen/openNetVM-Scripts/pktgen-config.lua

print_header "Pktgen installed"
