#!/bin/bash

set -e

. helper-functions.sh
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
    echo "ERROR: Missing second argument, PR_ID!"
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
    echo "ERROR: Missing fifth argument, POST_MSG!"
    exit 1
else
    POST_MSG=$5
fi

. $1 # source the variables from config file

print_header "Checking Required Variables"


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

print_header "Posting Message in Comments on GitHub"
python3 post-msg.py $GITHUB_CREDS "{\"id\": $PR_ID,\"request\":\"$REQUEST\"}" $REPO_OWNER $REPO_NAME "$POST_MSG"
check_exit_code "ERROR: Failed to post results to GitHub"
