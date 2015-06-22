## Simple OpenNetVM

This is a simplified version of onvm that only supports a single NF.

### Execution
To run the program:
```
cd samples/simple_onvm
sudo ./onvm_mgr/onvm_mgr/x86_64-native-linuxapp-gcc/onvm_mgr -c 6 -n 4 -- -p 1 -n  1

sudo ./onvm_nf/onvm_nf/x86_64-native-linuxapp-gcc/onvm_nf -c 8 -n 4 --proc-type=auto -- -n 0
```

On another server run `pktgen-dpdk` to verify that it works.
