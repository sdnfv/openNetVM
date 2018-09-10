#!/bin/bash

function usage {
        echo "$0 CPU-LIST SERVICE-ID CLIENT_IFACE SERVER_IFACE SERVER_CONFIG [-p PRINT] [-n NF-ID]"
        echo "$0 5 1 dpdk0 dpdk1 example.conf --> core 5, with Service ID 1, dpdk0 as Client interface, dpdk1 as Server interface, example.conf config"
        echo "$0 3,7,9 1 server.conf 1000 --> cores 3,7, and 9, with Service ID 1, server.conf as backend conf, and Print Rate of 1000"
        exit 1
}

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
cpu=$1
service=$2
client=$3
server=$4
config=$5

shift 5

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

exec sudo $SCRIPTPATH/build/load_balancer -l $cpu -n 3 --proc-type=secondary -- -r $service $instance -- -c $client -s $server -f $config $print
