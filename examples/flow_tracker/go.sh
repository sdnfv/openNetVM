#!/bin/bash

function usage {
        echo "$0 CPU-LIST SERVICE-ID DST-SERVICE-ID [PRINT]"
        echo "$0 -F CONFIG_FILE -- -- -d DST_SERVICE-ID [-p PRINT]"
        echo ""
        echo "$0 4, 1, 2 --> Core 4, service ID 1, and forwards packets to service ID 2"
        echo "$0 -F example_config.json 2 --> loads from example_config.json and forwards to service ID 2"
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
  exec sudo $SCRIPTPATH/build/flow_tracker -F $config "$@"
elif [ $1 = "-F" ]
then
  usage
fi

cpu=$1
service=$2
dst=$3

shift 3

if [ -z $service ]
then
  usage
fi

if [ -z $dst ]
then
  usage
fi

while getopts ":p:" opt; do
  case $opt in
    p) print="-p $OPTARG";;
    \?) echo "Unknown option -$OPTARG" && usage
    ;;
  esac
done

exec sudo $SCRIPTPATH/build/app/flow_tracker -l $cpu -n 3 --proc-type=secondary -- -r $service -- -d $dst $print
