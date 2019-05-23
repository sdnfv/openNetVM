#./go.sh 0,1,2,3,4,5,6 3
./go.sh 0,1,2,3,4,5,6 3 2>&1 | tee onvm_stat_${1}.txt
#echo $! >> mgr_pid.txt
