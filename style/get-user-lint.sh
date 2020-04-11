#!/bin/bash

# A script to run linter functions on updated C/python/bash files.
# This script is triggered by github actions when pull request are created.
# Users may also run this script to lint their file changes before pushing their code.
# Usage: run ./style/get-c-lint.sh python OR ./style/get-c-lint.sh c OR ./style/get-c-lint.sh shell OR ./style/get-c-lint.sh cppcheck

# Check if user is in main git repository and correct number of arguments are passed in.
if [ ! -d .git ]; then
    echo 'Please run in main git repository. Example Usage: ./style/get-c-lint.sh python';
    exit 1
fi

if [ "$#" -ne 1 ]; then
    echo 'Illegal number of parameters. Example usage: run ./style/get-user-lint.sh python'
    exit 1
fi

# If the remote upstream has already been set, update url. Else add upstream then fetch.
git remote set-url upstream https://github.com/sdnfv/openNetVM.git > /dev/null 2>&1 || git remote add upstream https://github.com/sdnfv/openNetVM.git
git fetch upstream

# Create output log file.
linter_out="linter_out.txt"
touch $linter_out

case "$1" in

"python")  echo $'Running Python Lint.\n'
    python3 "style/get-user-lint.py" "pylint" "*.py"
    ;;
"c")  echo $'Running C Lint.\n'
    python3 "style/get-user-lint.py" "python ./style/gwclint.py" "*.c *.cpp *.h | grep -v 'cJSON' | grep -v 'ndpi'"
    ;;
"cppcheck") echo $'Running cppcheck Lint.\n'
    python3 "style/get-user-lint.py" "cppcheck" "*.c *.cpp *.h | grep -v 'cJSON' | grep -v 'ndpi'"
   ;;
"shell") echo $'Running Shell Lint.\n'
    python3 "style/get-user-lint.py" "shellcheck -f gcc" "*.sh"
    ;;
*) echo $'Illegal parameter. Example usage: run ./style/get-user-lint.sh python'
   exit 1
   ;;
esac

# Check if the linter_out file is empty. If it's not empty, errors are present and the lint exits 1.
if [[ -s $linter_out ]]; then
    echo $'\nYour lint results below.\n'
    cat "$linter_out"
    rm $linter_out
    exit 1
else
    echo $'\nNo lint errors.'
    rm $linter_out
    exit 0
fi
