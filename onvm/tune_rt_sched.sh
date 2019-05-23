#default_value $1=100
echo $1 > /proc/sys/kernel/sched_rr_timeslice_ms
