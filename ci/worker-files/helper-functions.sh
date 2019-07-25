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
    id=$($DPDK_DEVBIND --status | grep -e "if=$1" | cut -f 1 -d " ")
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

    if [[ ! -z $MTCP_IFACE ]]
    then
        # bring iface up so onvm install can't bind it
        sudo ifconfig $MTCP_IFACE 11.0.0.17 up
    fi

    if [[ ! -z $PKT_IFACE ]]
    then
        # dummy so setup_environment binds nothing to dpdk
        sudo ifconfig $PKT_IFACE 11.0.0.17 up
    fi

    cd ../ 
    . ./scripts/install.sh
 
    # helper for binding interfaces 
    export DPDK_DEVBIND=$RTE_SDK/usertools/dpdk-devbind.py

    if [[ ! -z $PKT_IFACE ]]
    then
        # bring pktgen interface down, and set it up
        bind_nic_from_iface $PKT_IFACE
        # disable flow table lookup for faster results
        sed -i "/ENABLE_FLOW_LOOKUP\=1/c\\ENABLE_FLOW_LOOKUP=0" ~/repository/onvm/onvm_mgr/Makefile
    fi
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
