#!/bin/bash
#usage: ./get_sched_and_perf_stat.sh >out.txt 2>&1

#for sched in c b r f
i=0
for sched in r c r b
do
        echo "****************************************************"
        echo "<START> For Scheduler type = $sched   <START> "
	if [ $i -gt 1 ]; then
		echo "Tuning RR to 1ms"
		./tune_rt_sched.sh 1
	else
		echo "For RR with 100ms"
		./tune_rt_sched.sh 100
	fi
	
	((i++))
        ./set_sched_type.sh $sched
        sleep 5
        #pidstat -C "aes|bridge|forward|monitor|basic|speed|perf|pidstat" -lrsuwh 1 5 &        
        pidstat -C "aes|bridge|forward|monitor|basic|speed|flow|chain" -lrsuwh 1 10 & 
	sleep 1       
	perf stat --cpu=8  -d -d -d -r 10 sleep 1
        perf sched record --cpu=8 sleep 2
	perf sched latency -s max
 
        sleep 3
        echo " <END> For Scheduler type = $sched  <END>"
        sleep 2
        echo "****************************************************"
done
        echo "reset to CFS Scheduler"
        ./set_sched_type.sh c
