## Simple OpenNetVM

This is a version of onvm that supports multiple NFs and multiple ports.

### Execution
To run the program:
```
cd onvm
sudo ./onvm_mgr/onvm_mgr/x86_64-native-linuxapp-gcc/onvm_mgr -c 6 -n 4 -- -p 1 -n 1
```
On an other terminal:
```
cd examples/basic_monitor
sudo ./monitor/monitor -c 8 -n 4 --proc-type=auto -- -n 0
```

On another server run `pktgen-dpdk` to verify that it works.
