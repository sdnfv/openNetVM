#!/bin/bash

RAW_DEVICES=$1
HUGE=$2
ONVM=$3
NAME=$4

DEVICES=()

if [ -z $NAME ]
then
        echo -e "sudo ./docker.sh DEVICES HUGEPAGES ONVM NAME\n"
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

sudo docker run -it --privileged ${DEVICES[@]} -v /var/run:/var/run -v $HUGE:$HUGE -v $ONVM:/openNetVM --name=$NAME ubuntu /bin/bash
