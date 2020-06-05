#!/bin/bash

function usage {
        echo "$0 CPU-LIST PORTMASK NF-COREMASK [-r NUM-SERVICES] [-d DEFAULT-SERVICE] [-s STATS-OUTPUT] [-p WEB-PORT-NUMBER] [-z STATS-SLEEP-TIME]"
        # this works well on our 2x6-core nodes
        echo "$0 0,1,2,3 3 0xF0 --> cores 0,1,2 and 3 with ports 0 and 1, with NFs running on cores 4,5,6,7"
        echo -e "\tCores will be used as follows in numerical order:"
        echo -e "\t\tRX thread, TX thread, ..., TX thread for last NF, Stats thread"
        echo -e "$0 0,1,2,3 3 0xF0 -s web"
        echo -e "\tRuns ONVM the same way as above, but prints statistics to the web browser"
        echo -e "$0 0,1,2,3 3 0xF0 -s web -p 9000"
        echo -e "\tRuns OVNM the same as above, but runs the web stats on port 9000 instead of defaulting to 8080"
        echo -e "$0 0,1,2,3 3 0xF0 -s stdout"
        echo -e "\tRuns ONVM the same way as above, but prints statistics to stdout"
        echo -e "$0 0,1,2,3 3 0xF0 -s stdout -c"
        echo -e "\tRuns ONVM the same way as above, but enables shared cpu support"
        echo -e "$0 0,1,2,3 3 0xF0 -s stdout -t 42"
        echo -e "\tRuns ONVM the same way as above, but shuts down after 42 seconds"
        echo -e "$0 0,1,2,3 3 0xF0 -s stdout -l 64"
        echo -e "\tRuns ONVM the same way as above, but shuts down after receiving 64 million packets"
        echo -e "$0 0,1,2,3 3 0xF0 -s stdout -v"
        echo -e "\tRuns ONVM the same way as above, but prints statistics to stdout in extra verbose mode"
        echo -e "$0 0,1,2,3 3 0xF0 -s stdout -vv"
        echo -e "\tRuns ONVM the same way as above, but prints statistics to stdout in raw data dump mode"
        echo -e "$0 0,1,2,3 3 0xF0 -a 0x7f000000000 -s stdout"
        echo -e "\tRuns ONVM the same way as above, but adds a --base-virtaddr dpdk parameter to overwrite default address"
        echo -e "$0 0,1,2,3 3 0xF0 -r 10 -d 2"
        echo -e "\tRuns ONVM the same way as above, but limits max service IDs to 10 and uses service ID 2 as the default"
        exit 1
}

ports=$1
nf_cores=$2

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
verbosity=1
# Initialize base virtual address to empty.
virt_addr=""

# only check for duplicate manager if not in Docker container
if [[ -n $(pgrep -u root -f "/onvm_mgr/.*/onvm_mgr") ]] && ! grep -q "docker" /proc/1/cgroup
then
    echo "Manager cannot be started while another is running"
    exit 1
fi

shift 2

if [ -z "$nf_cores" ]
then
    usage
fi

ports_detected=$("$RTE_SDK"/usertools/dpdk-devbind.py --status-dev net | sed '/Network devices using kernel driver/q' | grep -c "drv")
if [[ $ports_detected -lt $ports ]]
then
    echo "Error: Invalid port mask. Insufficient NICs bound."
    exit 1
fi

while getopts "a:r:d:s:t:l:p:z:cvm:" opt; do
    case $opt in
        a) virt_addr="--base-virtaddr=$OPTARG";;
        r) num_srvc="-r $OPTARG";;
        d) def_srvc="-d $OPTARG";;
        s) stats="-s $OPTARG";;
        t) ttl="-t $OPTARG";;
        l) packet_limit="-l $OPTARG";;
        p) web_port="$OPTARG";;
        z) stats_sleep_time="-z $OPTARG";;
        c) shared_cpu_flag="-c";;
        v) verbosity=$((verbosity+1));;
        m) cpu=$OPTARG;;
        \?) echo "Unknown option -$OPTARG" && usage
            ;;
    esac
done

# Check validity of core input
three_cores="^[0-9],[0-9],[0-9]$"
two_cores="^[0-9],[0-9]$"
# Check for CPU core argument
if [ -z "$cpu" ]
then
    echo "WARNING: Using default CPU cores 0,1,2"
    echo ""
    cpu="0,1,2"
elif [[ ! $cpu =~ $three_cores ]] && [[ ! $cpu =~ $two_cores ]]
then
    echo "Error: Invalid CPU cores. openNetVM accepts two or three cores"
    exit 1
fi

# Trim 0x from NF mask
nf_cores_trimmed=${nf_cores:2}
# Convert nf_cores to compare to cpu
nf_cores_trimmed=$(echo "obase=10; ibase=16; $nf_cores_trimmed" | bc)
# Convert cpu to separate
cpu_separated=$(echo $cpu | tr "," "\n")
for core in $cpu_separated
do
    if (( nf_cores_trimmed & (1 << (core)) ))
    then
        echo "WARNING: Manager and NF cores overlap."
        echo ""
    fi
done

verbosity_level="-v $verbosity"

# If base virtual address has not been set by the user, set to default.
if [[ -z $virt_addr ]]
then
    echo "Base virtual address set to default 0x7f000000000"
    virt_addr="--base-virtaddr=0x7f000000000"
fi

if [ "${stats}" = "-s web" ]
then
    cd ../onvm_web/ || usage
    if [ -n "${web_port}" ]
    then
        . start_web_console.sh -p "${web_port}"
    else
        . start_web_console.sh
    fi

    cd "$ONVM_HOME"/onvm || usage
fi

sudo rm -rf /mnt/huge/rtemap_*
# watch out for variable expansion
# shellcheck disable=SC2086
sudo "$SCRIPTPATH"/onvm_mgr/"$RTE_TARGET"/onvm_mgr -l "$cpu" -n 4 --proc-type=primary ${virt_addr} -- -p ${ports} -n ${nf_cores} ${num_srvc} ${def_srvc} ${stats} ${stats_sleep_time} ${verbosity_level} ${ttl} ${packet_limit} ${shared_cpu_flag}

if [ "${stats}" = "-s web" ]
then
    echo "Killing web stats running with PIDs: $ONVM_WEB_PID, $ONVM_WEB_PID2"
    kill "$ONVM_WEB_PID"
    kill "$ONVM_WEB_PID2"
fi
