#!/bin/bash

function usage {
        echo "$0 -k PORTMASK -n NF-COREMASK [-m MANAGER CORES] [-r NUM-SERVICES] [-d DEFAULT-SERVICE] [-s STATS-OUTPUT] [-p WEB-PORT-NUMBER] [-z STATS-SLEEP-TIME]"
        # this works well on our 2x6-core nodes
        echo "$0 -k 3 -n 0xF0 --> cores 0,1,2, with ports 0 and 1, with NFs running on cores 4,5,6,7"
        echo -e "\tBy default, cores will be used as follows in numerical order:"
        echo -e "\t\tRX thread, TX thread, ..., TX thread for last NF, Stats thread"
        echo -e "$0 -k 3 -n 0xF0 -m 2,3,4"
        echo -e "\tRuns ONVM the same way as above, but manually configures cores 2, 3 and 4 to be used as stated"
        echo -e "$0 -k 3 -n 0xF0 -m 2,3,4 -s web"
        echo -e "\tRuns ONVM the same way as above, but prints statistics to the web browser"
        echo -e "$0 -k 3 -n 0xF0 -m 2,3,4 -s web -p 9000"
        echo -e "\tRuns OVNM the same as above, but runs the web stats on port 9000 instead of defaulting to 8080"
        echo -e "$0 -k 3 -n 0xF0 -m 2,3,4 -s stdout"
        echo -e "\tRuns ONVM the same way as above, but prints statistics to stdout"
        echo -e "$0 -k 3 -n 0xF0 -m 2,3,4 -s stdout -c"
        echo -e "\tRuns ONVM the same way as above, but enables shared cpu support"
        echo -e "$0 -k 3 -n 0xF0 -m 2,3,4 -s stdout -c -j"
        echo -e "\tRuns ONVM the same way as above, but allows ports to send and receive jumbo frames"
        echo -e "$0 -k 3 -n 0xF0 -m 2,3,4 -s stdout -t 42"
        echo -e "\tRuns ONVM the same way as above, but shuts down after 42 seconds"
        echo -e "$0 -k 3 -n 0xF0 -m 2,3,4 -s stdout -l 64"
        echo -e "\tRuns ONVM the same way as above, but shuts down after receiving 64 million packets"
        echo -e "$0 -k 3 -n 0xF0 -m 2,3,4 -s stdout -v"
        echo -e "\tRuns ONVM the same way as above, but prints statistics to stdout in extra verbose mode"
        echo -e "$0 -k 3 -n 0xF0 -m 2,3,4 -s stdout -vv"
        echo -e "\tRuns ONVM the same way as above, but prints statistics to stdout in raw data dump mode"
        echo -e "$0 -k 3 -n 0xF0 -m 2,3,4 -a 0x7f000000000 -s stdout"
        echo -e "\tRuns ONVM the same way as above, but adds a --base-virtaddr dpdk parameter to overwrite default address"
        echo -e "$0 -k 3 -n 0xF0 -m 2,3,4 -r 10 -d 2"
        echo -e "\tRuns ONVM the same way as above, but limits max service IDs to 10 and uses service ID 2 as the default"
        exit 1
}

# User can still use legacy syntax for backwards compatibility. Check syntax of input
# Check validity of core input
core_check="^([0-8]+,){2}([0-8]+)(,[0-8]+)*$"
port_check="^[0-9]+$"
nf_check="^0x[0-9A-F]+$"
flag_check="^-[a-z]$"
# Check for argument matches
[[ $1 =~ $core_check ]]
if [[ -n ${BASH_REMATCH[0]} ]]
then
    core_match=true
else
    core_match=false
fi
[[ $2 =~ $port_check ]]
if [[ -n ${BASH_REMATCH[0]} ]]
then
    port_match=true
else
    port_match=false
fi
[[ $3 =~ $nf_check ]]
if [[ -n ${BASH_REMATCH[0]} ]]
then
    nf_match=true
else
    nf_match=false
fi

# Make sure someone isn't inputting the cores incorrectly and they are using legacy syntax
# The flag regex check ensures that the user is trying to input a flag, which is an indicator that they are using the new syntax
if [[ ! $1 =~ $flag_check ]] && (! $core_match || ! $port_match || ! $nf_match)
then
    if ( ! $core_match && ( $port_match || $nf_match ))
    then
        # This verifies that the user actually tried to input the cores but did so incorrectly
        echo "Error: Invalid Manager Cores. Proper syntax: $0 <cores> <port mask> <NF cores> [OPTIONS]"
        echo "Example: $0 0,1,2 1 0xF8 -s stdout"
        exit 1
    elif ( ! $port_match && ( $core_match || $nf_match ))
    then
        # This verifies that the user actually tried to input the port mask but did so incorrectly
        echo "Error: Invalid Port Mask. Proper syntax: $0 <cores> <port mask> <NF cores> [OPTIONS]"
        echo "Example: $0 0,1,2 1 0xF8 -s stdout"
        exit 1
    elif ( ! $nf_match && ($core_match || $port_match ))
    then
        # This verifies that the user actually tried to input the NF core mask but did so incorrectly
        echo "Error: Invalid NF Core Mask. Proper syntax: $0 <cores> <port mask> <NF cores> [OPTIONS]"
        echo "Example: $0 0,1,2 1 0xF8 -s stdout"
        exit 1
    # We should never get here, but just a catch-all situation
    else
        echo "Error: Invalid input."
        echo ""
        usage
    fi
elif $core_match && $port_match && $nf_match
then
    script_name=$0
    cpu=$1
    ports=$2
    nf_cores=$3
    shift 3
fi

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

while getopts "a:r:d:s:t:l:p:z:cvm:k:n:j" opt; do
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
        m)
            # User is trying to set CPU cores but has already done so using legacy syntax
            if [[ -n $cpu ]]
            then
                echo "Error: Cannot use manual manager core flag with legacy syntax. Proper syntax: $script_name <cores> <port mask> <NF cores> [OPTIONS]"
                echo "Example: $script_name 0,1,2 1 0xF8 -s stdout"
                exit 1
            else
                cpu=$OPTARG
            fi;;
        k)
            # User is trying to set port mask but has already done so using legacy syntax
            if [[ -n $ports ]]
            then
                echo "Error: Cannot use manual port mask flag with legacy syntax. Proper syntax: $script_name <cores> <port mask> <NF cores> [OPTIONS]"
                echo "Example: $script_name 0,1,2 1 0xF8 -s stdout"
                exit 1
            else
                ports=$OPTARG
            fi;; 
        n) 
            # User is trying to set NF core mask but has already done so using legacy syntax
            if [[ -n $nf_cores ]]
            then
                echo "Error: Cannot use manual NF core mask flag with legacy syntax. Proper syntax: $script_name <cores> <port mask> <NF cores> [OPTIONS]"
                echo "Example: $script_name 0,1,2 1 0xF8 -s stdout"
                exit 1
            else
                nf_cores=$OPTARG
            fi;;
        j) jumbo_frames_flag="-j";;
        \?) echo "Unknown option -$OPTARG" && usage
            ;;
    esac
done

# Verify that dependency bc is installed
if [[ -z $(command -v bc) ]]
then
    echo "Error: bc is not installed. Install using:"
    echo "  sudo apt-get install bc"
    echo "See dependencies for more information"
    exit 1
fi

# Check for ports flag
if [ -z "$ports" ]
then
    echo "Error: Port Mask not set. openNetVM requires that a Port Mask be set using -k <port mask> using a hexadecimal number (without 0x)"
    echo ""
    usage
# Check port mask
elif [[ ! $ports =~ $port_check ]]
then
    echo "Error: Invalid port mask. Check input and try again."
    echo ""
    usage
fi

# Check for nf_cores flag
if [ -z "$nf_cores" ]
then
    echo "Error: NF Core Mask not set. openNetVM requires that a NF Core Mask be set using -n <nf mask> using a hexadecimal number (starting with 0x)"
    echo ""
    usage
# Check NF core mask
elif [[ ! $nf_cores =~ $nf_check ]]
then
    echo "Error: Invalid NF core mask. Check input and try again."
    echo ""
    usage
fi

# Check for CPU core flag
if [ -z "$cpu" ]
then
    echo "INFO: Using default CPU cores 0,1,2"
    echo ""
    cpu="0,1,2"
# Check CPU cores
elif [[ ! $cpu =~ $core_check ]]
then
    echo "Error: Invalid CPU cores. openNetVM accepts 3 or more cores. Check input and try again."
    echo ""
    usage
fi

if [ -z "$nf_cores" ]
then
    usage
fi

# Convert the port mask to binary
# Using bc where obase=2 indicates the output is base 2 and ibase=16 indicates the output is base 16
ports_bin=$(echo "obase=2; ibase=16; $ports" | bc)
# Splice out the 0's from the binary numbers. The result is only 1's. Example: 1011001 -> 1111
ports_bin="${ports_bin//0/}"
# The number of ports is the length of the string of 1's. Using above example: 1111 -> 4
count_ports="${#ports_bin}"

ports_detected=$("$RTE_SDK"/usertools/dpdk-devbind.py --status-dev net | sed '/Network devices using kernel driver/q' | grep -c "drv")
if [[ $ports_detected -lt $count_ports ]]
then
    echo "Error: Invalid port mask. Insufficient NICs bound."
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
        break
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
    cd "$ONVM_HOME"/onvm_web/ || usage
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
sudo "$SCRIPTPATH"/onvm_mgr/"$RTE_TARGET"/onvm_mgr -l "$cpu" -n 4 --proc-type=primary ${virt_addr} -- -p ${ports} -n ${nf_cores} ${num_srvc} ${def_srvc} ${stats} ${stats_sleep_time} ${verbosity_level} ${ttl} ${packet_limit} ${shared_cpu_flag} ${jumbo_frames_flag}

if [ "${stats}" = "-s web" ]
then
    echo "Killing web stats running with PIDs: $ONVM_WEB_PID, $ONVM_WEB_PID2"
    kill "$ONVM_WEB_PID"
    kill "$ONVM_WEB_PID2"
fi
