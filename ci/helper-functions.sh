#!/bin/bash

# simple print function that takes a single argument
# prints the argument as a header wrapped in dashes and pipes
print_header() {
    # check that the argument exists
    if [ -z "$1" ]
    then
        echo "No argument supplied to print_header!"
        return 1
    fi

    # get the string length for formatting
    strlen=${#1}

    echo ""

    # echo first line of dashes
    echo -n "--"
    for i in `eval echo {1..$strlen}`
    do
        echo -n "-"
    done
    echo "--"

     # echo the argument
    echo -n "| "
    echo -n $1
    echo " |"

    # echo the second line of dashes
    echo -n "--"
    for i in `eval echo {1..$strlen}`
    do
        echo -n "-"
    done
    echo "--"
    echo ""

    sleep 1
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

    echo $RTE_SDK

    sudo sh -c "echo 0 > /proc/sys/kernel/randomize_va_space"

    cd ../
    pwd
    . ./scripts/install.sh
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

# obtains core config in cores.out file
obtain_core_config() {
    cd scripts
    ./corehelper.py > cores.out
}

# checks if a command has failed (exited with code != 0)
# if it does, print the error message, exit the build, and post to github
check_exit_code(){
    if [ $? -ne 0 ]
    then
        echo $1
        cd $SCRIPT_LOC
    python3 post-msg.py $GITHUB_CREDS "{\"id\": $PR_ID,\"request\":\"$REQUEST\",\"linter\": 1}" $REPO_OWNER $REPO_NAME "Error: $1"
    exit 1
    fi
}

# runs the linter
run_linter() {
    for fn in $(git ls-files *.c *.cpp *.h | grep -v "cJSON" | grep -v "ndpi"); do
        python style/gwclint.py --verbose=5 $fn 2>&1 | grep -v "Done" | grep -v "Total errors found: 0" >> $1
    done
}
