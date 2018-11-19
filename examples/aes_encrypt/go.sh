#!/bin/bash

function usage {
        echo "$0 CPU-LIST SERVICE-ID DST [-p PRINT] [-n NF-ID]"
        echo "$0 -F CONFIG [remaining NF args]"
        echo ""
        echo "$0 3 0 1 --> core 3, Service ID 0, and forwards to service 1"
        echo "$0 -F example_config.json -- -- -d 1 --> loads settings from example_config.json, forwards to service ID 1"
        echo "$0 3,7,9 1 2 --> cores 3,7, and 9 with Service ID 1, and forwards to service id 2"
        echo "$0 -p 1000 -n 6 3,7,9 1 2 --> cores 3,7, and 9 with Service ID 1, Print Rate of 1000, instance ID 6, and forwards to service id 2"
        exit 1
}

SCRIPT=$(readlink -f "$0");
SCRIPTPATH=$(dirname "$SCRIPT");

if [ -z $1 ]
then
  usage
fi

if [ $1 = "-F" ]
then
  config=$2
  shift 2
  echo $config
  echo "$@"
  exec sudo $SCRIPTPATH/build/app/aesencrypt -F $config "$@"
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

exec sudo $SCRIPTPATH/build/app/aesencrypt -l $cpu -n 3 --proc-type=secondary -- -r $service $instance -- -d $dst $print
