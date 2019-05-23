#!/bin/bash
#usage: ./get_sched_and_perf_stat.sh >out.txt 2>&1

for sched in c b r f
do
        echo "****************************************************"
        echo "<START> For Scheduler type = $sched   <START> "
        ./set_sched_type.sh $sched
        sleep 10
        echo " <END> For Scheduler type = $sched  <END>"
        echo "****************************************************"
done
        echo "reset to CFS Scheduler"
        ./set_sched_type.sh c
