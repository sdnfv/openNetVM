#!/bin/bash
# install all dependencies only once
set -e
. helper-functions.sh
SCRIPT_LOC=$(pwd)

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

print_header "Done Installing, running CI"
# run the web server with the input arguments
python3 webhook-receiver.py $1 $2 $3 $4
