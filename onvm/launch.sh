rm core
ulimit -c unlimited
./kill_all.sh
#script

#./tune_cfs.sh 1 5
./go.sh 0,1,2,3,4,5,6,7 7 -s stdout
#./go.sh 0,1,2,3,4,5,6 3 -s web -p 9000

#./go.sh 0,1,2,3,4,5,6 3
#./go.sh 0,1,2,3,4,5,6,7 3
#./go.sh 0,1,2,3,4,5,6 3 2>&1 | tee onvm_stat_${1}.txt
#echo $! >> mgr_pid.txt
echo $?
