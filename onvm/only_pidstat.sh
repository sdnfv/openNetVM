#!/bin/bash
#usage: ./get_sched_and_perf_stat.sh >out.txt 2>&1

        echo " $1 ... ****************************************************"
        #pidstat -C "aes|bridge|forward|monitor|basic|speed|perf|pidstat" -lrsuwh 1 5 &        
        pidstat -C "aes|bridge|forward|monitor|basic|speed|flow|chain" -lrsuwh 1 10 & 
	sleep 1       
        echo "$ ... ****************************************************"
