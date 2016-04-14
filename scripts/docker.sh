#!/bin/bash

#                        openNetVM
#                https://sdnfv.github.io
#
# OpenNetVM is distributed under the following BSD LICENSE:
#
# Copyright(c)
#       2015-2016 George Washington University
#       2015-2016 University of California Riverside
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
# * Neither the name of Intel Corporation nor the names of its
#   contributors may be used to endorse or promote products derived
#   from this software without specific prior written permission.
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

RAW_DEVICES=$1
HUGE=$2
ONVM=$3
NAME=$4
DIR=$5

DEVICES=()

if [ -z $NAME ]
then
        echo -e "sudo ./docker.sh DEVICES HUGEPAGES ONVM NAME [DIRECTORY]\n"
        echo -e "\te.g. sudo ./docker.sh /dev/uio0,/dev/uio1 /hugepages /root/openNetVM Basic_Monitor_NF"
        echo -e "\t\tThis will create a container with two NIC devices, uio0 and uio1,"
        echo -e "\t\thugepages mapped from the host's /hugepage directory and openNetVM"
        echo -e "\t\tmapped from /root/openNetVM and it will name it Basic_Monitor_NF"
        exit 1
fi

IFS=','

for DEV in $RAW_DEVICES
do
        DEVICES+=("--device=$DEV:$DEV")
done

if [ -z $DIR ]
then
	sudo docker run -it --privileged ${DEVICES[@]} -v /var/run:/var/run -v $HUGE:$HUGE -v $ONVM:/openNetVM --name=$NAME ubuntu /bin/bash
else
	sudo docker run -it --privileged ${DEVICES[@]} -v /var/run:/var/run -v $HUGE:$HUGE -v $ONVM:/openNetVM -v $DIR:/$(basename $DIR) --name=$NAME ubuntu /bin/bash
fi
