echo 0 > /proc/sys/kernel/randomize_va_space
sysctl -w kernel.randomize_va_space=0
sysctl -p
