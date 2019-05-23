#!/bin/bash 

#pidstat -C "bridge|forward" -uwh 5
#pidstat -C "bridge|forward" -lrsuwh 5 5 >>plog.txt 
#pidstat -C "bridge|forward" -lrsuwh 5 5 | tee plog.txt
num_nfs=$1
testcase=$2
 
if [ -z $testcase ]
then
    echo "Usage:$0 [num_nfs] [testcase]"
    exit 1
fi
 
#num_basic_nf=$((num_nfs/4*0))
#num_bridge=$((num_nfs))
num_basic_nf=$((num_nfs -1)) #$((num_nfs/4*0))
num_bridge=1 #$((num_nfs))
#types="basic_nf bridge monitor forward"
types="bridge forward"

ctx_num=0
vol_ctx_num=0
invol_ctx_num=0
 
#result=/home/skulk901/ctx_dumps/$testcase
result=./$testcase

startTime=`cat ${result}/date.start`
endTime=`cat ${result}/date.end`
interval=$(($endTime-$startTime))

function calculate {
    valueType=$1
    file=$2
    init=`grep $valueType ${file}.start | awk '{print $3}'`
    end=`grep $valueType ${file}.end | awk '{print $3}'`
    echo $((end-init))
}
 
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
        file=${result}/${type}_${i}
        tmp_ctx_num=`calculate nr_switches $file` 
        ctx_num=$((ctx_num+tmp_ctx_num))
         
        tmp_vol_ctx_num=`calculate nr_voluntary_switches $file`
        vol_ctx_num=$((vol_ctx_num+tmp_vol_ctx_num))
 
        tmp_invol_ctx_num=`calculate nr_involuntary_switches $file`
        invol_ctx_num=$((invol_ctx_num+tmp_invol_ctx_num))
    done
 
done
 
echo $testcase $ctx_num $vol_ctx_num $invol_ctx_num >> ctx.total
echo $testcase $(($ctx_num*1000000000/$interval)) $(($vol_ctx_num*1000000000/$interval)) $(($invol_ctx_num*1000000000/$interval)) >> ctx.avg
