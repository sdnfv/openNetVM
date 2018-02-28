#!/bin/bash

function usage {
        echo "$0 CPU-LIST SERVICE-ID [-p PRINT] [-n NF-ID]"
        echo "$0 3 0 --> core 3, Service ID 0"
        echo "$0 3,7,9 1 --> cores 3,7, and 9 with Service ID 1"
        echo "$0 3,7,9 1 1000 --> cores 3,7, and 9 with Service ID 1 and Print Rate of 1000"
        exit 1
}

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
cpu=$1
service=$2

shift 2

if [ -z $service ]
then
    usage
fi

while getopts ":p:n:" opt; do
  case $opt in
    p) print="-p $OPTARG";;
    n) instance="-n $OPTARG";;
    \?) echo "Unknown option -$OPTARG" && usage
    ;;
  esac
done

exec sudo $SCRIPTPATH/build/app/flow_table -l $cpu -n 3 --proc-type=secondary -- -r $service $instance -- $print
