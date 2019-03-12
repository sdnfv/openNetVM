#!/bin/sh

#                    openNetVM
#      https://github.com/sdnfv/openNetVM
#
# BSD LICENSE
#
# Copyright(c)
#          2015-2016 George Washington University
#          2015-2016 University of California Riverside
#          2010-2014 Intel Corporation.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in
# the documentation and/or other materials provided with the
# distribution.
# The name of the author may not be used to endorse or promote
# products derived from this software without specific prior
# written permission.
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

# These are the interfaces that you do not want to use for Pktgen-DPDK
BLACK_LIST="-b 0000:05:00.0 -b 0000:05:00.1"

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")

# Path for pktgen
PKTGEN_HOME="$SCRIPTPATH/../pktgen-dpdk/"

# Path for pktgen binary
PKTGEN_BUILD="./app/x86_64-native-linuxapp-gcc/pktgen"

# Path for pktgen config
PKTGEN_CONFIG="$SCRIPTPATH/pktgen-config.lua"

if [ "$#" -lt 1 ] ; then
    echo "Pass an argument for port count"
    echo "Example usage: sudo bash run-pktgen.sh 1"
    exit 0
fi

PORT_NUM=$1

echo "Starting pktgen"
if [ $PORT_NUM  -eq "2" ]; then
    (cd $PKTGEN_HOME && sudo $PKTGEN_BUILD -c 0xff -n 3 $BLACK_LIST -- -p 0x3 $PORT_MASK -P -m "[1:2].0, [3:4].1" -f $PKTGEN_CONFIG)
elif [ $PORT_NUM -eq "1" ]; then
    (cd $PKTGEN_HOME && sudo $PKTGEN_BUILD -c 0xff -n 3 $BLACK_LIST -- -p 0x1 $PORT_MASK -P -m "[1:2].0" -f $PKTGEN_CONFIG)
else
    echo "Only supports 1 or 2 ports"
    exit 0
fi

echo "Pktgen done"
