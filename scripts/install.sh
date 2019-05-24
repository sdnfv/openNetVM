#! /bin/bash

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

set -e

# A script to configure openNetVM
# Expected to be run as scripts/install.sh
# CONFIGURATION (via environment variable):
#  - Make sure $RTE_TARGET and $RTE_SDK are correct (see install docs)
#  - Set $ONVM_NUM_HUGEPAGES to control the number of pages created
#  - Set $ONVM_SKIP_FSTAB to not add huge fs to /etc/fstab

#Print a table with enviromental variable locations
echo "----------------------------------------"
echo "ONVM Environment Variables:"
echo "----------------------------------------"
echo "RTE_SDK: $RTE_SDK"
echo "RTE_TARGET: $RTE_TARGET"
echo "ONVM_NUM_HUGEPAGES: $ONVM_NUM_HUGEPAGES"
echo "ONVM_SKIP_HUGEPAGES: $ONVM_SKIP_HUGEPAGES"
echo "ONVM_SKIP_FSTAB: $ONVM_SKIP_FSTAB"
echo "----------------------------------------"

if [ -z "$RTE_TARGET" ]; then
    echo "Please export \$RTE_TARGET. Or try running this without sudo."
    exit 1
fi

if [ -z "$RTE_SDK" ]; then
    echo "Please export \$RTE_SDK"
    exit 1
fi

# Validate sudo access
sudo -v

# Ensure we're working relative to the onvm root directory
if [ $(basename $(pwd)) == "scripts" ]; then
    cd ..
fi

# Set state variables
start_dir=$(pwd)

if [ -z "$ONVM_HOME" ]; then
    echo "Please export \$ONVM_HOME and set it to $start_dir"
    exit 1
fi

# Source DPDK helper functions
. $ONVM_HOME/scripts/dpdk_helper_scripts.sh

set +e
remove_igb_uio_module
set -e

# Compile dpdk
cd $RTE_SDK
echo "Compiling and installing dpdk in $RTE_SDK"

# Adding ldflags.txt output for mTCP compatibility
if grep "ldflags.txt" $RTE_SDK/mk/rte.app.mk > /dev/null
then
    :
else
    sed -i -e 's/O_TO_EXE_STR =/\$(shell if [ \! -d \${RTE_SDK}\/\${RTE_TARGET}\/lib ]\; then mkdir -p \${RTE_SDK}\/\${RTE_TARGET}\/lib\; fi)\nLINKER_FLAGS = \$(call linkerprefix,\$(LDLIBS))\n\$(shell echo \${LINKER_FLAGS} \> \${RTE_SDK}\/\${RTE_TARGET}\/lib\/ldflags\.txt)\nO_TO_EXE_STR =/g' $RTE_SDK/mk/rte.app.mk
fi

sleep 1
make config T=$RTE_TARGET
make T=$RTE_TARGET -j 8
make install T=$RTE_TARGET -j 8

# Refresh sudo
sudo -v

cd $start_dir

# Setup/Check for free HugePages if the user wants to
if [ -z "$ONVM_SKIP_HUGEPAGES" ]; then
    set_numa_pages
fi

grep -m 1 "huge" /etc/fstab | cat
# Only add to /etc/fstab if user wants it
if [ ${PIPESTATUS[0]} != 0 ] && [ -z "$ONVM_SKIP_FSTAB" ]; then
    echo "Adding huge fs to /etc/fstab"
    sleep 1
    sudo sh -c "echo \"huge /mnt/huge hugetlbfs defaults 0 0\" >> /etc/fstab"
fi

# Configure local environment
echo "Configuring environment"
sleep 1
scripts/setup_environment.sh

echo "ONVM INSTALL COMPLETED SUCCESSFULLY"

