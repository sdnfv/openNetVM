#!/bin/bash

function usage {
        echo "$0 CPU-LIST SERVICE-ID ROUTER_CONF [-p PRINT] [-n NF-ID]"
        echo "$0 3 1 router.conf --> cores 3 with Service ID 1, and router config 'router.conf'"
        exit 1
}

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
cpu=$1
service=$2
config=$3

shift 3

if [ -z $config ]
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

exec sudo $SCRIPTPATH/build/app/nf_router -l $cpu -n 3 --proc-type=secondary -- -r $service $instance -- -f $config $print
