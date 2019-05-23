#!/bin/bash 
# Example: ./tune_cfs.sh 0
# Example: ./tune_cfs.sh 1
# Example: ./tune_cfs.sh [any > 1] [sched_latency_ns] [sched_min_granularity_ns] [sched_wakeup_granularity_ns]

#CFS TUNABLE PARAMETERS
#    sched_min_granularity_ns (16000000): Minimum preemption granularity for processor-bound tasks. Tasks are guaranteed to run for this minimum time before they are preempted.
#    sched_latency_ns (80000000): Period over which CFQ tries to fairly schedule the tasks on the runqueue. All of the tasks on the runqueue are guaranteed to be scheduled once within this period.
#    sched_wakeup_granularity_ns (20000000): Ability of tasks being woken to preempt the current task. The smaller the value, the easier it is for the task to force the preemption.

#sysctl parameters:
#sysctl kernel.sched_min_granularity_ns 
#sysctl kernel.sched_latency_ns
#sysctl kernel.sched_wakeup_granularity_ns 

#proc parameters:
#cat /proc/sys/kernel/sched_min_granularity_ns
#cat /proc/sys/kernel/sched_latency_ns
#cat /proc/sys/kernel/sched_wakeup_granularity_ns

#IBM AIX SERVER RECOMMENDED CFS PARAMETERS
#kernel.sched_min_granularity_ns = 100000
#kernel.sched_wakeup_granularity_ns = 25000
#kernel.sched_latency_ns = 1000000

#DEFAULTS ON LINUX SERVER:
#kernel.sched_min_granularity_ns = 3000000
#kernel.sched_latency_ns = 24000000
#kernel.sched_wakeup_granularity_ns = 4000000

def_sched_min_granularity_ns=0
def_sched_latency_ns=0
def_sched_wakeup_granularity_ns=0
do_read_defaults() {
        def_sched_min_granularity_ns=`cat /proc/sys/kernel/sched_min_granularity_ns`
        def_sched_latency_ns=`cat /proc/sys/kernel/sched_latency_ns`
        def_sched_wakeup_granularity_ns=`cat /proc/sys/kernel/sched_wakeup_granularity_ns`
        
        echo "$def_sched_min_granularity_ns and `sysctl kernel.sched_min_granularity_ns`"
        echo "$def_sched_latency_ns and `sysctl kernel.sched_latency_ns`"
        echo "$def_sched_wakeup_granularity_ns and `sysctl kernel.sched_wakeup_granularity_ns`"
}
do_read_defaults

do_set_cfs_restore_defaults() {
        echo "********* Applying DEFAULT CFS Parameters **********"
        echo 3000000 > /proc/sys/kernel/sched_min_granularity_ns
        echo 24000000 > /proc/sys/kernel/sched_latency_ns
        echo 4000000 > /proc/sys/kernel/sched_wakeup_granularity_ns

        do_read_defaults
}

do_set_cfs_ibm_optimal() {
        echo "********* Applying IBM AIX CFS Parameters **********"
        echo 100000 > /proc/sys/kernel/sched_min_granularity_ns
        echo 1000000 > /proc/sys/kernel/sched_latency_ns
        echo 25000 > /proc/sys/kernel/sched_wakeup_granularity_ns

        do_read_defaults
}

do_set_predef_vals () {

        case $inp_mode in
        "2")
                inp_sched_latency_ns=500000
                inp_sched_min_granularity_ns=250000
                inp_sched_wakeup_granularity_ns=25000
        ;;
        "3")
                inp_sched_latency_ns=250000
                inp_sched_min_granularity_ns=100000
                inp_sched_wakeup_granularity_ns=100000
        ;;
        "4")
                inp_sched_latency_ns=100000
                inp_sched_min_granularity_ns=100000
                inp_sched_wakeup_granularity_ns=25000
        ;;
        "5")
                inp_sched_latency_ns=100000
                inp_sched_min_granularity_ns=100000
                inp_sched_wakeup_granularity_ns=1000000
        ;;
        esac
        
        echo "********* Applying Predefined CFS Parameters $inp_mode: min_gran=$inp_sched_min_granularity_ns, lat=$inp_sched_latency_ns, wake_gran=$inp_sched_wakeup_granularity_ns **********"
        echo $inp_sched_min_granularity_ns > /proc/sys/kernel/sched_min_granularity_ns
        echo $inp_sched_latency_ns > /proc/sys/kernel/sched_latency_ns
        echo $inp_sched_wakeup_granularity_ns > /proc/sys/kernel/sched_wakeup_granularity_ns

        do_read_defaults
}

#Set the Latency Round Bounds between 10ns and 1sec] Must be atleast 4 times greater than min_gran_ns. 
#Practical reasons val should be [ 250ns and 100ms ]
#Note: System rejects val below 100microsec [100000 ... ]
min_sched_latency_ns=10
max_sched_latency_ns=1000000000

#Set the per process run time (similar to slice) [2ns to 250ms ]: must be atleast 4 times less than sched_latency 
#Practical reasons val should be [ 250ns and 100ms ]
#Note: System rejects val below 100microsec [100000 ... ]
min_sched_min_granularity_ns=100000
max_sched_min_granularity_ns=250000000

#Set the per process wakeup [smaller => easier to evict, bigger => tougher to evict] compared to sched_min_granularity_ns
#Practical reason keep lower than sched_min_granularity_ns
#System accepts any value between [1... ]
min_sched_wakeup_granularity_ns=2
max_sched_wakeup_granularity_ns=250000000

#Assign Input args to 
inp_mode=$1
inp_sched_latency_ns=$2
inp_sched_min_granularity_ns=$3
inp_sched_wakeup_granularity_ns=$4

if [ -z $inp_mode ] ; then
        exit 0
elif [ $inp_mode -eq "0" ] ; then
        do_set_cfs_restore_defaults
        exit 0
elif [ $inp_mode -eq "1" ] ; then
        do_set_cfs_ibm_optimal
        exit 0
elif [  $inp_mode -gt "1" -a   $inp_mode -le "5" ] ; then
        do_set_predef_vals
        exit 0
fi

#Latency=Round Duration
if [ -z $inp_sched_latency_ns ]; then
        inp_sched_latency_ns=$def_sched_latency_ns
elif [ $inp_sched_latency_ns -lt $min_sched_latency_ns ] ; then
        inp_sched_latency_ns=$def_sched_latency_ns
elif [ $inp_sched_latency_ns -gt $max_sched_latency_ns ] ; then
        inp_sched_latency_ns=$def_sched_latency_ns
fi

if [ -z $inp_sched_min_granularity_ns ]; then
        inp_sched_min_granularity_ns=$def_sched_min_granularity_ns
elif [ $inp_sched_min_granularity_ns -lt $min_sched_min_granularity_ns ] ; then
        inp_sched_min_granularity_ns=$def_sched_min_granularity_nss
elif [ $inp_sched_min_granularity_ns -gt $max_sched_min_granularity_ns ] ; then
        inp_sched_min_granularity_ns=$def_sched_min_granularity_ns
fi


if [ -z $inp_sched_wakeup_granularity_ns ]; then
        inp_sched_wakeup_granularity_ns=$def_sched_wakeup_granularity_ns
elif [ $inp_sched_wakeup_granularity_ns -lt $min_sched_wakeup_granularity_ns ] ; then
        inp_sched_wakeup_granularity_ns=$def_sched_wakeup_granularity_ns
elif [ $inp_sched_wakeup_granularity_ns -gt $max_sched_wakeup_granularity_ns ] ; then
        inp_sched_wakeup_granularity_ns=$def_sched_wakeup_granularity_ns
fi

do_set_cfs_params() {
        
        echo "********* Applying new CFS Parameters **********"
        echo "sched_min_granularity_ns: $inp_sched_min_granularity_ns"
        echo "sched_latency_ns: $inp_sched_latency_ns"   
        echo "sched_wakeup_granularity_ns: $inp_sched_wakeup_granularity_ns"

        echo "$inp_sched_min_granularity_ns" > /proc/sys/kernel/sched_min_granularity_ns
        echo "$inp_sched_latency_ns" > /proc/sys/kernel/sched_latency_ns
        echo "$inp_sched_wakeup_granularity_ns" > /proc/sys/kernel/sched_wakeup_granularity_ns
        
        #sysctl -w kernel.sched_min_granularity_ns $inp_sched_min_granularity_ns
        #sysctl -w kernel.sched_latency_ns $inp_sched_latency_ns
        #sysctl -w kernel.sched_wakeup_granularity_ns $$inp_sched_wakeup_granularity_ns
        #sysctl -p

        do_read_defaults
}
do_set_cfs_params

