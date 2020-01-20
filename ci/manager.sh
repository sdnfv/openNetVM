#!/bin/bash

# exit on error
set -e

# source helper functions file
. helper-manager-functions.sh
# SCRIPT_LOC is used in helper-manager-functions.sh
# shellcheck disable=SC2034
SCRIPT_LOC=$(pwd)

print_header "Validating Config File and Sourcing Variables"

if [[ -z "$1" ]]
then
    echo "ERROR: Missing first argument, path to config file!"
    exit 1
fi

if [[ ! -f $1 ]]
then
    echo "ERROR: Could not find config file at given path!"
    exit 1
fi

if [[ -z "$2" ]]
then
    echo "ERROR: Missing second argument, PR ID!"
    exit 1
else
    PR_ID=$2
fi

if [[ -z "$3" ]]
then
    echo "ERROR: Missing third argument, Repo name!"
    exit 1
else
    REPO_NAME=$3
fi

if [[ -z "$4" ]]
then
    echo "ERROR: Missing fourth argument, Request body!"
    exit 1
else
    REQUEST=$4
fi

if [[ -z "$5" ]]
then
    echo "ERROR: Missing fifth argument, Run mode type!"
    exit 1
else
    RUN_MODE=$5
fi

. "$1" # source the variables from config file

print_header "Checking Required Variables"

if [[ -z "$WORKER_LIST" ]]
then
    echo "ERROR: WORKER_LIST not set"
    exit 1
fi

if [[ -z "$GITHUB_CREDS" ]]
then
    echo "ERROR: GITHUB_CREDS not provided"
    exit 1
fi

if [[ -z "$REPO_OWNER" ]]
then
    echo "ERROR: REPO_OWNER not provided"
    exit 1
fi

print_header "Cleaning up Old Results"

sudo rm -f ./*.txt
sudo rm -rf ./*stats
sudo rm -rf repository

print_header "Checking Worker and GitHub Creds Exist"

if [[ ! -f $GITHUB_CREDS ]]
then
    echo "ERROR: GITHUB_CREDS is not a valid file"
    exit 1
fi

print_header "Posting on Github that CI is starting"
python3 post-msg.py "$GITHUB_CREDS" "{\"id\": $PR_ID,\"request\":\"$REQUEST\"}" "$REPO_OWNER" "$REPO_NAME" "Your results will arrive shortly"
check_exit_code "ERROR: Failed to post initial message to GitHub"

for worker_tuple in "${WORKER_LIST[@]}"
do
    tuple_arr=($worker_tuple)
    worker_key_file="${tuple_arr[1]}"
    if [[ ! -f $worker_key_file ]]
    then
        echo "ERROR: Worker key file does not exist"
        exit 1
    fi
done

# turn off error checking
set +e

print_header "Fetching and Checking Out Pull Request"
python3 clone-and-checkout-pr.py "$GITHUB_CREDS" "{\"id\": $PR_ID}" "$REPO_OWNER" "$REPO_NAME"
check_exit_code "ERROR: Failed to fetch and checkout pull request"

print_header "Running Linter"
cd repository
rm -f ../linter-output.txt
run_linter ../linter-output.txt
cd ..

if [[ "$RUN_MODE" -eq "1" ]]
then
    # only run linter and develop checks if unauthorized
    print_header "Posting Results in Comment on GitHub"
    python3 post-msg.py "$GITHUB_CREDS" "{\"id\": $PR_ID,\"request\":\"$REQUEST\",\"linter\": 1,\"review\": 1}" "$REPO_OWNER" "$REPO_NAME" "Run successful see results:"
    check_exit_code "ERROR: Failed to post results to GitHub"
    exit 0
fi

print_header "Preparing Workers"
for worker_tuple in "${WORKER_LIST[@]}"
do
    tuple_arr=($worker_tuple)
    worker_ip="${tuple_arr[0]}"
    worker_key_file="${tuple_arr[1]}"
    worker_user="${tuple_arr[2]}"
    python3 prepare-worker.py "$worker_ip" "$worker_key_file" "$worker_user"
done

sleep 10 # wait 10 seconds for reboot to take effect

for worker_tuple in "${WORKER_LIST[@]}"
do
    tuple_arr=($worker_tuple)
    worker_ip="${tuple_arr[0]}"
    print_header "Waiting for reboot of $worker_ip to Complete"
    while ! ping -c 1 "$worker_ip" &>/dev/null; do :; done
    check_exit_code "ERROR: Waiting for reboot of $worker_ip failed"
done

print_header "All Workers Prepared"

sleep 10 # give an extra 10 seconds for ssh/scp to come online

print_header "Copying ONVM files to Workers"
for worker_tuple in "${WORKER_LIST[@]}"
do
    tuple_arr=($worker_tuple)
    worker_ip="${tuple_arr[0]}"
    worker_key_file="${tuple_arr[1]}"
    worker_user="${tuple_arr[2]}"
    # make sure the config file is updated with the correct run mode
    sed -i "/WORKER_MODE*/c\\WORKER_MODE=\"${RUN_MODE}\"" worker_files/worker-config
    # create directory for scp
    mkdir temp
    # put all files in one temporary folder for one scp
    cp -r ./"$worker_ip"/* ./repository ./worker_files/* temp
    scp -i "$worker_key_file" -oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null -r ./temp/* "$worker_user"@"$worker_ip":
    check_exit_code "ERROR: Failed to copy ONVM files to $worker_ip"
    # get rid of the temp folder now for next worker
    sudo rm -rf temp
done

print_header "Running Workloads on Workers"
for worker_tuple in "${WORKER_LIST[@]}"
do
    tuple_arr=($worker_tuple)
    worker_ip="${tuple_arr[0]}"
    worker_key_file="${tuple_arr[1]}"
    worker_user="${tuple_arr[2]}"
    python3 run-workload.py "$worker_ip" "$worker_key_file" "$worker_user"
    check_exit_code "ERROR: Script failed on $worker_ip"
done

print_header "Obtaining Performance Results from all workers"
for worker_tuple in "${WORKER_LIST[@]}"
do
    tuple_arr=($worker_tuple)
    worker_ip="${tuple_arr[0]}"
    worker_key_file="${tuple_arr[1]}"
    worker_user="${tuple_arr[2]}"
    # get the benchmarks for each node (some servers are faster)
    . ./"$worker_ip"/benchmarks
    # TODO: this will overwrite results if we have more than 1 worker, investigate this case
    if [[ "$RUN_MODE" -eq "0" ]]
    then
        # fetch pktgen stats 
        fetch_files "$worker_key_file" "$worker_ip" pktgen_stats "$worker_user"
        python3 pktgen-analysis.py ./"$worker_ip".pktgen_stats "$worker_ip" pktgen_summary.stats "$AVG_PKTGEN_SPEED"
        check_exit_code "Failed to parse Pktgen stats"
        # fetch speed_tester stats
        fetch_files "$worker_key_file" "$worker_ip" speed_stats "$worker_user"
        python3 speed-tester-analysis.py ./"$worker_ip".speed_stats "$worker_ip" speed_summary.stats "$AVG_SPEED_TESTER_SPEED"
        check_exit_code "Failed to parse Speed Tester stats"
    else
        # only fetch speed tester stats if mode is not 0
        fetch_files "$worker_key_file" "$worker_ip" speed_stats "$worker_user"
        python3 speed-tester-analysis.py ./"$worker_ip".speed_stats "$worker_ip" speed_summary.stats "$AVG_SPEED_TESTER_SPEED"
        check_exit_code "Failed to parse Speed Tester stats"
    fi
    check_exit_code "ERROR: Failed to analyze results from $worker_ip"
done

print_header "Posting Results in Comment on GitHub"
python3 post-msg.py "$GITHUB_CREDS" "{\"id\": $PR_ID,\"request\":\"$REQUEST\",\"linter\": 1,\"results\": 1,\"review\": 1}" "$REPO_OWNER" "$REPO_NAME" "Run successful see results:"
check_exit_code "ERROR: Failed to post results to GitHub"

print_header "Finished Executing"
exit 0
