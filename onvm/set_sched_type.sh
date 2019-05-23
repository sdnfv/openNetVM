#!/bin/bash 
# Example: ./set_sched_type fifo 30
# Example: ./set_sched_type r 30
# Example: ./set_sched_type o
class_default="o" # choose as 1) FIFO, fifo, F, f, 2) ROUNDROBIN, RR, R, r, 3) Other, NORMAL, O, o
prio_default="0"
class=${1}
prio=${2}

if [ -z $class ]; then
        class=" -o "
        prio=" -p 0 "
elif [ $class == "FIFO" ] || [ $class == "fifo" ] || [ $class == "F" ] || [ $class == "f" ]; then
        class=" -f "
        if [ -z $2  ]; then
                prio=" -p 50 "
        else
                prio=" -p $2 "
        fi
elif [ $class  == "ROUNDROBIN" ] || [ $class ==  "RR" ] || [ $class == "rr" ] || [ $class == "R" ] || [ $class == "r" ]; then
        class=" -r "
        if [ -z $2  ]; then
                prio=" -p 50 "
        else
                prio=" -p $2 "
        fi
elif [ $class ==  "BATCH" ] || [ $class == "batch" ] || [  $class == "B" ] || [ $class == "b" ]; then
        class=" -b "
        prio=" -p 0 "
elif [ $class ==  "OTHER" ] || [ $class == "other" ] || [ $class == "NORMAL" ] || [ $class == "normal" ] || [  $class == "O" ] || [ $class == "o" ]  ||
     [ $class == "CFS" ] || [ $class == "cfs" ] || [  $class == "C" ] || [ $class == "c" ]   ; then
        class=" -o "
        prio=" -p 0 "
fi

#re_n=^-?[0-9]+([.][0-9]+)?$
#re1=^[0-9]+([.][0-9]+)?$
echo "class=$class"
echo "prio=$prio"

re='^[0-9]+$'
pids=`fuser -v -m /mnt/huge | head -n 1 | awk '{print }'`
for pid in $pids
do
  if [ $pid == "kernel" ]; then
    echo "Skip Kernel Process: $pid"
    continue
  elif ! [[ $pid =~ $re ]]; then
    echo "Not changing for Unknown Process: $pid"
    continue
  else
    echo "Changing Process: $class $prio $pid"
    chrt $class $prio $pid
  fi
   
done

