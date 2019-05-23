#!/bin/bash 
 
num_nfs=$1
testcase=$2
suffix=$3
 
if [ -z $suffix ]
then
    echo "Usage:$0 [num_nfs] [testcase] [suffix]"
    exit 1
fi
 
num_basic_nf=$((num_nfs -1)) #$((num_nfs/4*0))
num_bridge=1 #$((num_nfs))
#types="basic_nf bridge monitor forward"
types="bridge forward"
 
#result=/home/skulk901/ctx_dumps/$testcase
result=./$testcase

mkdir -p $result
 
for type in $types
do
    if [ $type == "basic_nf" ]
    then
        num_nfs=$num_basic_nf
    elif [ $type == "bridge" ]
    then
        num_nfs=$num_bridge
    else
        num_nfs=$num_basic_nf
    fi
 
    echo $type $num_nfs
 
    for i in `seq $num_nfs`
    do
    pid=`ps aux | grep $type | grep  -v "sudo" | grep -v "perf" | grep "proc" | awk '{print $2}' | head -n $i | tail -n 1`
    echo $type, $pid
    echo "${result}"
    echo "grep -A 2 "nr_switches" /proc/${pid}/sched >> ${result}/${type}_${i}.ctx"
    echo "qwer1234" | sudo grep -A 2 "nr_switches" /proc/${pid}/sched  >> ${result}/${type}_${i}.${suffix} 
    done
done
 
date +%s%N >> ${result}/date.${suffix}
