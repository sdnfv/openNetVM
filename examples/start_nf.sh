#!/bin/bash

function usage {
        echo "Run the example NF in one of the following ways:"
        echo "$0 NF-NAME SERVICE-ID [remaining NF args]"
        echo "$0 NF-NAME DPDK_ARGS -- ONVM_ARGS -- NF_ARGS"
        echo "$0 NF-NAME -F config.json [other args]"
        echo ""
        echo "$0 speed_tester 1 -d 1  --> Speed tester NF, with Service ID 1, and forwards to service ID 1"
        echo "$0 speed_tester -l 5 -- -m -r 3 -- -d 2  --> Speed tester NF on core 5 (manually assigned), with Service ID 3, and forwards to service ID 2"
        echo "$0 monitor -l 0 -- -r 1 -n 3 --  --> Basic monitor NF on core 4, with Instance ID 3, Service ID 1"
        echo "$0 monitor -F config.json  --> Basic monitor NF with arguments extracted from the config.json file"
        exit 1
}

# 2 args: NF_NAME + (service_id or -F config_name)
if [ "$#" -lt 2 ]; then
  echo "ERROR: Missing required arguments"
  usage
  exit 1
fi

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
NF_NAME=$1
NF_PATH=$SCRIPTPATH/$NF_NAME
BINARY=$NF_PATH/build/app/$NF_NAME
DPDK_BASE_ARGS="-n 3 --proc-type=secondary"
# For simple mode, only used for initial dpdk startup
DEFAULT_CORE_ID=0

if [ ! -f $BINARY ]; then
  echo "ERROR: NF executable not found, $BINARY doesn't exist"
  echo "Please verify NF binary name and run script from the NF folder"
  exit 1
fi

shift 1

# Config launch, when using the config we don't really parse any other args
if [ "$1" = "-F" ]
then
  config=$2
  shift 2
  exec sudo $BINARY -F $config "$@"
fi

# Check if -- is present, if so parse dpdk/onvm specific args
dash_dash_cnt=0
non_nf_arg_cnt=0
for i in "$@" ; do
  if [[ dash_dash_cnt -lt 2 ]] ; then
        non_nf_arg_cnt=$((non_nf_arg_cnt+1))
  fi
  if [[ $i == "--" ]] ; then
    dash_dash_cnt=$((dash_dash_cnt+1))
  fi
done

# Spaces before $@ are required otherwise it swallows the first arg for some reason
if [[ $dash_dash_cnt -ge 2 ]]; then
  DPDK_ARGS="$DPDK_BASE_ARGS $(echo " ""$@" | awk -F "--" '{print $1;}')"
  ONVM_ARGS="$(echo " ""$@" | awk -F "--" '{print $2;}')"
  # Move to NF arguments
  shift ${non_nf_arg_cnt}
elif [[ $dash_dash_cnt -eq 0 ]]; then
  # Dealing with required args shared by all NFs
  service=$1
  shift 1

  DPDK_ARGS="-l $DEFAULT_CORE_ID $DPDK_BASE_ARGS"
  ONVM_ARGS="-r $service"
elif [[ $dash_dash_cnt -eq 1 ]]; then
  # Don't allow only one `--`
  echo "This script expects 0 or at least 2 '--' argument separators"
  usage
  exit 1
fi

exec sudo $BINARY $DPDK_ARGS -- $ONVM_ARGS -- "$@"
