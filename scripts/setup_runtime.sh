#! /bin/bash

#                        openNetVM
#                https://sdnfv.github.io
#
# OpenNetVM is distributed under the following BSD LICENSE:
#
# Copyright(c)
#       2015-2024 George Washington University
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
# Sets configuration items on the host system required to run openNetVM

arg_hugepages=true

# Basic check to make sure this script is running in the correct working
# directory.
if [ "$(basename "$(pwd)")" != "openNetVM" ]; then
    echo "Please run the installation script from the parent openNetVM directory"
fi

# Parse the passed arguments, and set the appropriate flags if a
# particular argument is detected
for arg in "$@"
do
    if [[ $arg == "--nohugepages" ]]; then
        arg_hugepages=false
        break
    fi
done

# Check sudo privileges
sudo -v 


# (1)
# Disable address space layout randomization (ASLR)
echo "- Disabling ASLR"
sudo sh -c "echo 0 > /proc/sys/kernel/randomize_va_space"


# (2)
# Disable hyperthreading
echo "- Disabling hyperthreading"

CPUS_TO_SKIP=" $(cat /sys/devices/system/cpu/cpu*/topology/thread_siblings_list | sed 's/[^0-9].*//' | sort | uniq | tr "\r\n" "  ") "
for CPU_PATH in /sys/devices/system/cpu/cpu[0-9]*; do
        CPU="$(echo "$CPU_PATH" | tr -cd "0-9")"
        echo "$CPUS_TO_SKIP" | grep " $CPU " > /dev/null
        if [ $? -ne 0 ]; then
            echo 0 > "$CPU_PATH"/online
        fi
done

lscpu | grep -i -E  "^CPU\(s\):|core|socket" 


# (3)
# Load the uio kernel module from dpdk-kmods
grep -m 1 "igb_uio" /proc/modules | cat
if [ "${PIPESTATUS[0]}" != 0 ]; then
    echo "- Loading uio kernel module"
    sudo modprobe uio
    sudo insmod ./subprojects/dpdk-kmods/linux/igb_uio/igb_uio.ko
else
    echo "- uio kernel module already loaded"
fi


# (4)
# Setup hugepages.
# This will be skipped if --nohugepages is passed in.
if [ "$arg_hugepages" = true ]; then
    echo "- Configuring hugepages"
    . ./scripts/dpdk_helper_scripts.sh
    set_numa_pages "$hp_count"
else
    echo "- Skipping hugepages"
fi