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

while getopts :d:w:c:D:h:o:n: option; do
  case $option in
    d)
      DIR=$OPTARG
      ;;
    w)
      DWD=$OPTARG
      ;;
    c)
      CMD=$OPTARG
      ;;
    D)
      RAW_DEVICES=$OPTARG
      ;;
    h)
      HUGE=$OPTARG
      ;;
    o)
      ONVM=$OPTARG
      ;;
    n)
      NAME=$OPTARG
      ;;
  esac
done

DEVICES=()

if [ -z $NAME ] || [ -z $HUGE ] || [ -z $ONVM ]
then
        echo -e "sudo ./docker.sh -h HUGEPAGES -o ONVM -n NAME [-D DEVICES] [-d DIRECTORY] [-w WORKINGDIRECTORY] [-c COMMAND]\n"
        echo -e "\te.g. sudo ./docker.sh -h /hugepages -o /root/openNetVM -n Basic_Monitor_NF -D /dev/uio0,/dev/uio1"
        echo -e "\t\tThis will create a container with two NIC devices, uio0 and uio1,"
        echo -e "\t\thugepages mapped from the host's /hugepage directory and openNetVM"
        echo -e "\t\tmapped from /root/openNetVM and it will name it Basic_Monitor_NF"
        exit 1
fi

if [ -z $DWD ]
then
        DWD="/"
fi

IFS=','

for DEV in $RAW_DEVICES
do
        DEVICES+=("--device=$DEV:$DEV")
done

if [ -z $CMD ]
then
        if [ -z $DIR ]
        then
                sudo docker run -it --network none --privileged ${DEVICES[@]} -v /var/run:/var/run -v $HUGE:$HUGE -v $ONVM:/openNetVM -w=$DWD --name=$NAME ubuntu:14.04 /bin/bash
        else
                sudo docker run -it --network none --privileged ${DEVICES[@]} -v /var/run:/var/run -v $HUGE:$HUGE -v $ONVM:/openNetVM -v $DIR:/$(basename $DIR) -w=$DWD --name=$NAME ubuntu:14.04 /bin/bash
        fi
else
        if [ -z $DIR ]
        then
                sudo docker run -d=true --network none --privileged ${DEVICES[@]} -v /var/run:/var/run -v $HUGE:$HUGE -v $ONVM:/openNetVM -w=$DWD --name=$NAME ubuntu:14.04 bash -c $CMD
        else
                sudo docker run -d=true --network none --privileged ${DEVICES[@]} -v /var/run:/var/run -v $HUGE:$HUGE -v $ONVM:/openNetVM -v $DIR:/$(basename $DIR) -w=$DWD --name=$NAME ubuntu:14.04 bash -c $CMD
        fi
fi
