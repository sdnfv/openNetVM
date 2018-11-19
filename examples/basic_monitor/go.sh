#!/bin/bash

function usage {
        echo "$0 CPU-LIST SERVICE-ID [-p PRINT] [-n NF-ID]"
        echo "$0 -F config.json -p 1"
        echo ""
        echo "$0 3 0 --> core 3, Service ID 0"
        echo "$0 3,7,9 1 --> cores 3,7, and 9 with Service ID 1"
        echo "$0 -F config.json -p 2 --> loads values from config.json and print rate of 2"
        echo "$0 -p 1000 -n 6 3,7,9 1 --> cores 3,7, and 9 with Service ID 1 and Print Rate of 1000 and instance ID 6"
        exit 1
}

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")

if [ -z $1 ]
then
  usage
fi

if [ $1 = "-F" ]
then
  config=$2
  shift 2
  exec sudo $SCRIPTPATH/build/app/monitor -F $config "$@"
fi

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

exec sudo $SCRIPTPATH/build/app/monitor -l $cpu -n 3 --proc-type=secondary -- -r $service $instance -- $print
