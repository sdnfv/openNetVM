#!/bin/bash

function usage {
        echo "$0 CPU-LIST SERVICE-ID DST [-p PRINT] [-n NF-ID]"
        echo "$0 -F CONFIG_FILE -- -- -d DST [-p PRINT]"
        echo ""
        echo "$0 3,7,9 1 2 --> cores 3,7, and 9, with Service ID 1, and forwards to service ID 2"
        echo "$0 -F example_config.json -- -- -d 2 --> loads settings from example_config.json and forwards to service id 2"
        echo "$0 3,7,9 1 2 1000 --> cores 3,7, and 9, with Service ID 1, forwards to service ID 2,  and Print Rate of 1000"
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
  exec sudo $SCRIPTPATH/build/NAT -F $config "$@"
elif [ $1 = "-F" ]
then
  usage
fi

cpu=$1
service=$2
dst=$3

shift 3

if [ -z $dst ]
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

exec sudo $SCRIPTPATH/build/app/NAT -l $cpu -n 3 --proc-type=secondary -- -r $service $instance -- -d $dst $print
