#! /bin/bash

# A script to configure dpdk environment variables on the cloudlab server
DPDK_PATH=/local/onvm/openNetVM/dpdk

if [ -z "$RTE_TARGET" ]; then
    export RTE_TARGET=x86_64-native-linuxapp-gcc
fi

if [ -z "$RTE_SDK" ]; then
    export RTE_SDK=$DPDK_PATH
fi


