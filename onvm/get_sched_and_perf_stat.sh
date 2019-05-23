#!/bin/bash
#usage: ./get_sched_and_perf_stat.sh >out.txt 2>&1

#for sched in c b r f
i=0
for sched in r c r b
do
	if [ $sched gt 1 ]; then
		./tune_rt_sched.sh 1
	elif [ "$sched" == "r" ]; then
		./tune_rt_sched.sh 100
	fi
        echo "****************************************************"
        echo "<START> For Scheduler type = $sched   <START> "
        ./set_sched_type.sh $sched
        sleep 5
        #pidstat -C "aes|bridge|forward|monitor|basic|speed|perf|pidstat" -lrsuwh 1 5 &        
        pidstat -C "aes|bridge|forward|monitor|basic|speed|flow|chain" -lrsuwh 1 10 &        
        #perf sched record --cpu=8 sleep 5
	#perf sched latency -s max
 
	perf stat --cpu=8  -d -d -d -r 10 sleep 1
        sleep 3
        echo " <END> For Scheduler type = $sched  <END>"
        sleep 2
        echo "****************************************************"
done
        echo "reset to CFS Scheduler"
        ./set_sched_type.sh c
