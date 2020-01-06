#!/bin/bash

#The go.sh script is a convinient way to run start_nf.sh without specifying NF_NAME

NF_DIR=${PWD##*/}

if [ ! -f ../start_nf.sh ]; then
  echo "ERROR: The ./go.sh script can only be used from the NF folder"
  echo "If running from other directory use examples/start_nf.sh"
  exit 1
fi

SCRIPT=$(readlink -f "$0")
MANAGER_PATH=$(dirname $(dirname "$SCRIPT"))

if [[ -z $(ps ww -u root | grep "$MANAGER_PATH/onvm/onvm_mgr/$RTE_TARGET/onvm_mgr") ]]
then
    echo "NF cannot start without a running manager"
    exit 1
fi

../start_nf.sh $NF_DIR "$@"
