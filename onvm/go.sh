#!/bin/bash

function usage {
        echo "$0 CPU-LIST PORTMASK [-r NUM-SERVICES] [-d DEFAULT-SERVICE] [-s STATS-OUTPUT] [-p WEB-PORT-NUMBER] [-z STATS-SLEEP-TIME]"
        # this works well on our 2x6-core nodes
        echo "$0 0,1,2,6 3 --> cores 0, 1, 2 and 6 with ports 0 and 1"
        echo -e "\tCores will be used as follows in numerical order:"
        echo -e "\t\tRX thread, TX thread, ..., TX thread for last NF, Stats thread"
        echo -e "$0 0,1,2,6 3 -s web"
        echo -e "\tRuns ONVM the same way as above, but prints statistics to the web browswer"
	echo -e "$0 0,1,2,6 3 -s web -p 9000"
	echo -e "\tRuns OVNM the same as above, but runs the web stats on port 9000 instead of defaulting to 8080"
        echo -e "$0 0,1,2,6 3 -s stdout"
        echo -e "\tRuns ONVM the same way as above, but prints statistics to stdout"
        echo -e "$0 0,1,2,6 3 -r 10 -d 2"
        echo -e "\tRuns ONVM the same way as above, but limits max service IDs to 10 and uses service ID 2 as the default"
        exit 1
}

cpu=$1
ports=$2

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")

shift 2

if [ -z $ports ]
then
    usage
fi

while getopts "v:r:d:s:p:z:" opt; do
    case $opt in
        v) virt_addr="--base-virtaddr=$OPTARG";;
        r) num_srvc="-r $OPTARG";;
        d) def_srvc="-d $optarg";;
        s) stats="-s $OPTARG";;
        p) web_port="$OPTARG";;
        z) stats_sleep_time="-z $OPTARG";;
        \?) echo "Unknown option -$OPTARG" && usage
            ;;
    esac
done

if [ "${stats}" = "-s web" ]
then
    cd ../onvm_web/
    if [ -n "${web_port}" ]
    then
        ./start_web_console.sh -p "${web_port}"
    else
        ./start_web_console.sh
    fi


    cd ../onvm/
fi

sudo rm -rf /mnt/huge/rtemap_*
sudo $SCRIPTPATH/onvm_mgr/$RTE_TARGET/onvm_mgr -l $cpu -n 4 --proc-type=primary ${virt_addr} -- -p ${ports} ${num_srvc} ${def_srvc} ${stats} ${stats_sleep_time}

if [ "${stats}" = "-s web" ]
then
    kill $ONVM_WEB_PID
fi
