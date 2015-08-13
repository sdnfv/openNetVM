## Bridge

This is a example using onvm that send every packets from one port to another.

### Execution
To run the program.
First run onvm manager and then :
```
cd examples/bridge
sudo ./bridge/x86_64-native-linuxapp-gcc/bridge -c 8 -n 4 --proc-type=auto -- -n 0
```
