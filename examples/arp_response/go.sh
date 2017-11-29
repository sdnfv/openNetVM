#!/bin/bash

function usage {
        echo "$0 CPU-LIST SERVICE-ID DST-SERVICE-ID -s SOURCE-IP"
        echo "$0 4 1 2 -s 10.0.0.31,11.0.0.31 --> core 4, Service ID 1, Destination ID 2, Port 0: 10.0.0.31, Port 1: 11.0.0.31"
        exit 1
}

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
cpu=$1
service=$2
dst=$3

shift 3

if [ -z $service ]
then
    usage
fi

while getopts ":s:p" opt; do
  case $opt in
    s) src_ip="-s $OPTARG";;
    p) enable_print="-p";;
    /?) echo "Unknown option -$OPTARG" && usage
    ;;
  esac
done

exec sudo $SCRIPTPATH/build/arp_response -l $cpu -n 3 --proc-type=secondary -- -r $service  -- -d $dst $src_ip $enable_print
