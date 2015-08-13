## Monitor

This is a example using onvm that print information about packets whose go through.

### Execution
To run the program.
First run onvm manager and then :
```
cd examples/basic_monitor
sudo ./basic_monitor/x86_64-native-linuxapp-gcc/monitor -c 8 -n 4 --proc-type=auto -- -n 0 -- -p 100
```
`-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.
