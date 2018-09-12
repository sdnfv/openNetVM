#!/bin/bash

function usage {
        echo "$0 CPU-LIST SERVICE-ID DST [-p PRINT] [-n NF-ID] [-a] [-s PACKET-SIZE] [-m DEST-MAC]
        [-c PACKET-NUMBER] [-l]"
        echo "$0 3,7,9 1 2 --> cores 3,7, and 9, with Service ID 1, and forwards to service ID 2"
        echo "$0 3,7,9 1 2 1000 --> cores 3,7, and 9, with Service ID 1, forwards to service ID 2,  and Print Rate of 1000"
        echo "$0 3,7,9 1 2 -s 32 --> cores 3,7, and 9, with Service ID 1, forwards to service ID 2, and packet size of 32"
        echo "$0 3,7,9 1 2 -s 32 -m aa:bb:cc:dd:ee:ff --> cores 3,7, and 9, with Service ID 1, forwards to service ID 2, packet size of 32, and destination MAC address of aa:bb:cc:dd:ee:ff"
        echo "$0 5 1 2 -o sample_trafic.pcap --> core 5, with Service ID 1, and forwards to service ID 2, replays sample_trafic.pcap packets (make sure to enable pcap functionality, check README for instructions)"
        echo "$0 5 1 2 -l --> core 5, with Service ID 1, forwards to service ID 2, measures latency"
        echo "Pass '-a' to signal the NF to use advanced ring manipulation"
        exit 1
}

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
cpu=$1
service=$2
dst=$3

shift 3

if [ -z $dst ]
then
    usage
fi

while getopts ":p:n:as:m:o:c:l" opt; do
  case $opt in
    p) print="-p $OPTARG";;
    n) instance="-n $OPTARG";;
    a) rings="-a";;
    s) size="-s $OPTARG";;
    m) dest_mac="-m $OPTARG";;
    o) pcap_filename="-o $OPTARG";;
    c) pkt_num="-c $OPTARG";;
    l) latency="-l";;
    \?) echo "Unknown option -$OPTARG" && usage
    ;;
  esac
done

exec sudo $SCRIPTPATH/build/app/speed_tester -l $cpu -n 3 --proc-type=secondary -- -r $service $instance -- -d $dst $print $rings $size $dest_mac $pcap_filename $pkt_num $latency
