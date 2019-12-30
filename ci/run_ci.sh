#!/bin/bash

# exit on error
set -e

# source helper functions file
. helper-manager-functions.sh
SCRIPT_LOC=$(pwd)

print_header "Validating Input Variables"

if [[ -z "$1" ]]
then
    echo "ERROR: Missing first argument, Host!"
    exit 1
else
    HOST=$1
fi

if [[ -z "$2" ]]
then
    echo "ERROR: Missing second argument, Flask server port!"
    exit 1
else
    PORT=$2
fi

if [[ -z "$3" ]]
then
    echo "ERROR: Missing third argument, Request Keyword!"
    exit 1
else
    KEYWORD=$3
fi

if [[ -z "$4" ]]
then
    echo "ERROR: Missing fourth argument, path to config file!"
    exit 1
else
    if [[ ! -f $4 ]]
    then
        echo "ERROR: Fourth argument, Config file, is not a file!"
        exit 1
    else
        CFG_NAME=$4
    fi
fi

# install all dependencies only once at beginning of CI
print_header "Installing dependencies"

sudo apt update
sudo apt install -y build-essential libssl-dev libffi-dev
sudo apt install -y python3
sudo apt install -y python3-pip

python3 -V
check_exit_code "ERROR: Python not installed"

pip3 -V
check_exit_code "ERROR: Pip not installed"

sudo -H pip3 install virtualenv --upgrade
check_exit_code "ERROR: virtualenv had trouble upgrading"

sudo -H pip3 install flask --upgrade
check_exit_code "ERROR: Flask not installed or failed to install"

sudo -H pip3 install --pre github3.py
check_exit_code "ERROR: GitHub3.py not installed or failed to install"

sudo -H pip3 install paramiko
check_exit_code "ERROR: Paramiko not installed or failed to install"

sudo -H pip3 install pexpect
check_exit_code "ERROR: Pexpect not installed or failed to install"

sudo -H pip3 install pycryptodome
check_exit_code "ERROR: Pycryptodome not installed or failed to install"

print_header "Done Installing, running CI"

# run the web server with the input arguments
python3 webhook-receiver.py $HOST $PORT $KEYWORD $CFG_NAME
