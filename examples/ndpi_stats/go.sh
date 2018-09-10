#!/bin/bash

function usage {
        echo "$0 CPU-LIST SERVICE-ID [-w RESULT_FILE_NAME]  [-d DST_NF] [-n NF-ID]" 
        echo "$0 5 1 --> core 5 with Service ID 1, packets sent OUT"
        echo "$0 5 1 -d 2 -w capt.txt --> core 5 with Service ID 1, destination NF 2, save results to capt.txt" 
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

while getopts ":d:w:n:" opt; do
  case $opt in
    d) destination="-d $OPTARG";; 
    w) write="-w $OPTARG";;
    n) instance="-n $OPTARG";;
    \?) echo "Unknown option -$OPTARG" && usage
    ;;
  esac
done

exec sudo $SCRIPTPATH/build/ndpi_stats -l $cpu -n 3 --proc-type=secondary -- -r $service $instance -- $destination $write
