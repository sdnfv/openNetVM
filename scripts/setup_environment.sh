#!/bin/bash

#                        openNetVM
#                https://sdnfv.github.io
#
# OpenNetVM is distributed under the following BSD LICENSE:
#
# Copyright(c)
#       2015-2017 George Washington University
#       2015-2017 University of California Riverside
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in
#   the documentation and/or other materials provided with the
#   distribution.
# * The name of the author may not be used to endorse or promote
#   products derived from this software without specific prior
#   written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# A script to configure your environment each boot
# Loads the uio kernel modules, shows NIC status
# CONFIGURATION (via environment variable):
#  - Ensure that $RTE_SDK and $RTE_TARGET are set (see install docs)
#  - Set ONVM_NIC to the name of the interface you want to bind (default p2p1)
#  - Can set ONVM_NIC_PCI to the PCI address of the interface (default 07:00.0)

set -e

DPDK_DEVBIND=$RTE_SDK/usertools/dpdk-devbind.py # for DPDK 17 and up
#DPDK_DEVBIND=$RTE_SDK/tools/dpdk-devbind.py # for DPDK 16.11
#DPK_DEVBIND=#RTE_SDK/tools/dpdk_nic_bind.py # for DPDK 2.2 D


# Confirm environment variables
if [ -z "$RTE_TARGET" ]; then
    echo "Please export \$RTE_TARGET"
    exit 1
fi

if [ -z "$RTE_SDK" ]; then
    echo "Please export \$RTE_SDK"
    exit 1
fi

# Ensure we're working relative to the onvm root directory
if [ $(basename $(pwd)) == "scripts" ]; then
    cd ..
fi

start_dir=$(pwd)

if [ -z "$ONVM_HOME" ]; then
    echo "Please export \$ONVM_HOME and set it to $start_dir"
    exit 1
fi

# Source DPDK helper functions
. $ONVM_HOME/scripts/dpdk_helper_scripts.sh

# Disable ASLR
sudo sh -c "echo 0 > /proc/sys/kernel/randomize_va_space"

# Setup/Check for free HugePages if the user wants to
if [ -z "$ONVM_SKIP_HUGEPAGES" ]; then
    set_numa_pages $hp_count
fi

# Verify sudo access
sudo -v

# Load uio kernel modules
grep -m 1 "igb_uio" /proc/modules | cat
if [ ${PIPESTATUS[0]} != 0 ]; then
    echo "Loading uio kernel modules"
    sleep 1
    cd $RTE_SDK/$RTE_TARGET/kmod
    sudo modprobe uio
    sudo insmod igb_uio.ko
else
    echo "IGB UIO module already loaded."
fi

# dpdk_nic_bind.py has been changed to dpdk-devbind.py to be compatible with DPDK 16.11
echo "Checking NIC status"
sleep 1
$DPDK_DEVBIND --status

echo "Binding NIC status"
if [ -z "$ONVM_NIC_PCI" ];then
    for id in $($DPDK_DEVBIND --status | grep -v Active | grep -e "10G" -e "10-Gigabit" | grep unused=igb_uio | cut -f 1 -d " ")
    do
        read -r -p "Bind interface $id to DPDK? [y/N] " response
        if [[ $response =~ ^([yY][eE][sS]|[yY])$ ]];then
            echo "Binding $id to dpdk"
            sudo $DPDK_DEVBIND -b igb_uio $id
        fi
    done
else
    # Auto binding example format: export ONVM_NIC_PCI=" 07:00.0  07:00.1 "
    for nic_id in $ONVM_NIC_PCI
    do
        echo "Binding $nic_id to DPDK"
        sudo $DPDK_DEVBIND -b igb_uio $nic_id
    done
fi

echo "Finished Binding"
$DPDK_DEVBIND --status

sudo bash $ONVM_HOME/scripts/no_hyperthread.sh

echo "Environment setup complete."
