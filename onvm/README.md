## OpenNetVM - Multi threads

This is a version of onvm that supports multiple NFs and multiple ports.
Also run manager with multi threads.

### Execution
To run the program:
```
cd onvm
sudo ./onvm_mgr/onvm_mgr/x86_64-native-linuxapp-gcc/onvm_mgr -c 1550 -n 4 -- -p 1 -n 1
```
`-c 1540` is a COREMASK, e.g. it will launch on core 6, 8, 10 and 12. Also 12 is
still available as it sleep most of the time.

On an other terminal:
```
cd examples/basic_monitor
sudo ./monitor/monitor -c 8 -n 4 --proc-type=auto -- -n 0
```

On another server run `pktgen-dpdk` to verify that it works.
