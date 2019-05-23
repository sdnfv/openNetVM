#!/bin/bash
#Use like: cpustat `pgrep processname` `pgrep otherprocessname` ...

pids=()
while [ $# != 0 ]; do
        pids=("${pids[@]}" "$1")
        shift
done

if [ -z "${pids[0]}" ]; then
        echo "Usage: $0 <pid1> [pid2] ..."
        exit 1
fi

for pid in "${pids[@]}"; do
        if [ ! -e /proc/$pid ]; then
                echo "Error: pid $pid doesn't exist"
                exit 1
        fi
done

while [ true ]; do
        echo -e "\033[H\033[J"
        for pid in "${pids[@]}"; do
                ps -p $pid -L -o pid,tid,psr,pcpu,comm=
        done
        sleep 1
done
