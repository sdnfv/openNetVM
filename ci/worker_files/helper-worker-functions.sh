#!/bin/bash

LOGFILE=worker.log
# remove previous log if it's over 300 lines
RETAIN_NUM_LINES=300
DPDK_DEVBIND=$RTE_SDK/usertools/dpdk-devbind.py

# print to stdout and the logfile
logsetup() {
    TMP=$(tail -n $RETAIN_NUM_LINES $LOGFILE 2>/dev/null) && echo "${TMP}" > $LOGFILE
    exec > >(tee -a $LOGFILE)
    exec 2>&1
}

log() {
    # check that the argument exists
    if [ -z "$1" ]
    then
        echo "No argument supplied to log!"
        return 1
    fi
 
    echo "[$(date --rfc-3339=seconds)]: $*"
}

# set up our log file
logsetup

# given a 10G NIC interface, bring down and bind to dpdk
bind_nic_from_iface() {
    sudo ifconfig $1 down
    id=$($DPDK_DEVBIND --status | grep -e "if=$1 drv=ixgbe" | cut -f 1 -d " ")
    sudo $DPDK_DEVBIND -b igb_uio $id
}

 # sets up dpdk, sets env variables, and runs the install script
install_env() {
    git submodule sync 
    git submodule update --init

    echo export ONVM_HOME=$(pwd) >> ~/.bashrc
    export ONVM_HOME=$(pwd)

    cd dpdk

    echo export RTE_SDK=$(pwd) >> ~/.bashrc
    export RTE_SDK=$(pwd)

    echo export RTE_TARGET=x86_64-native-linuxapp-gcc  >> ~/.bashrc
    export RTE_TARGET=x86_64-native-linuxapp-gcc

    echo export ONVM_NUM_HUGEPAGES=1024 >> ~/.bashrc
    export ONVM_NUM_HUGEPAGES=1024

    sudo sh -c "echo 0 > /proc/sys/kernel/randomize_va_space"

    # helper for binding interfaces
    export DPDK_DEVBIND=$RTE_SDK/usertools/dpdk-devbind.py

    for iface in $($DPDK_DEVBIND --status | grep -oP "if=\K(\w+)\sdrv=ixgbe" | cut -f 1 -d " ")
    do
        # bring all ixgbe interfaces up so install script can't bind
        sudo ifconfig $iface 11.0.0.1 up
    done

    cd ../ 
    . ./scripts/install.sh

    # bring client facing interface down for dpdk
    bind_nic_from_iface $CLIENT_IFACE
    # disable flow table lookup for faster results
    sed -i "/ENABLE_FLOW_LOOKUP\=1/c\\ENABLE_FLOW_LOOKUP=0" ~/repository/onvm/onvm_mgr/Makefile
}

# makes all onvm code
build_onvm() {
    cd onvm
    make clean && make
    cd ../

    cd examples
    make clean && make
    cd ../
}

# checks if a command has failed (exited with code != 0)
# if it does, print the error message, exit the build, and post to github
check_exit_code() {
    if [ $? -ne 0 ]
    then
        echo $1
        cd $SCRIPT_LOC
        exit 1
    fi
}
