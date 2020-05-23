#!/bin/bash

#The go.sh script is a convinient way to run start_nf.sh without specifying NF_NAME

NF_DIR=${PWD##*/}

if [ ! -f ../start_nf.sh ]; then
  echo "ERROR: The ./go.sh script can only be used from the NF folder"
  echo "If running from other directory use examples/start_nf.sh"
  exit 1
fi

# only check for running manager if not in Docker
if [[ -z $(pgrep -u root -f "/onvm/onvm_mgr/.*/onvm_mgr") ]] && ! grep -q "docker" /proc/1/cgroup
then
    echo "NF cannot start without a running manager"
    exit 1
fi

../start_nf.sh "$NF_DIR" "$@"
