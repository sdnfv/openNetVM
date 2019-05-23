#!/bin/bash 

#types="basic_nf bridge"

#for type in $types
#do
#num_lines=`ps aux | grep $type | grep -v "color" | wc -l`
#echo $num_lines

#for i in `seq $num_lines`
#do
#        echo "i=$i"
#        pid=`ps aux|grep $type | grep -v "color" | head -n 1 | awk '{print $2}'`
#        echo $pid
#        echo "19840115" | sudo kill -9 $pid
#done
#done

#re_n=^-?[0-9]+([.][0-9]+)?$
#re1=^[0-9]+([.][0-9]+)?$
re='^[0-9]+$'
pids=`fuser -v -m /mnt/huge | head -n 1 | awk '{print }'`
echo "Process IDs using DPDK/HUGE Pages: $pids"
for pid in $pids
do
  if [ $pid == "kernel" ]; then
    echo "Not killing Kernel Process: $pid"
    continue
  elif ! [[ $pid =~ $re ]]; then
    echo "Not killing Unknown Process: $pid"
    continue
  else
    echo "killing Process: $pid"
    sudo kill -9 $pid
  fi
   
done

max_clients=3
def_share_val=1024
cg_base_dir="/sys/fs/cgroup/cpu"
cg_name="nf_"
reset_cgroup_nf_shares() {
    for i in `seq 1 $max_clients`; do
    echo $def_share_val > $cg_base_dir/${cg_name}${i}/cpu.shares
    done
}
reset_cgroup_nf_shares
