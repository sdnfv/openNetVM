#!/bin/bash

function usage {
        echo "$0 CPU-LIST SERVICE-ID DST  [-a]"
        echo "$0 3,4,5,6,7 1 6 --> cores 3 as NF with SID 1 sending to itself, 4,5,6,7 as 4NFs (SID 6) sending to itself"
        echo "$0 3,4,5,6 1 4 -a --> cores 3,4,5,6 as 4NFs (SID 1) sending to 4 with advanced rings"
        exit 1
}

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
cpu=$1
service=$2
dst=$3

shift 3

while getopts "a" opt; do
  case $opt in
    a) rings="-a";;
    \?) echo "Unknown option -$OPTARG" && usage
    ;;
  esac
done

exec sudo $SCRIPTPATH/build/app/scaling -l $cpu -n 3 --proc-type=secondary -- -r $service -- -d $dst $rings
