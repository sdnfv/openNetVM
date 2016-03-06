## Simple Flow Table

This is a example with a simple flow table that matches a packet to a destination NF.  The lookup is based on the packet 5-tuple and the port (physical or NF) of where the packet came from.

## Notes:

```
# compile with debug flags:
make ONVM=~/openNetVM/onvm/ USER_FLAGS="-g -O0 -DDEBUG_PRINT"

# Run with script:
./go <CPU> <NF_ID>

# run directly:
sudo ./build/flow_table -c <CPU> -n 4 --proc-type=secondary -- -n <NF_ID>

# run in gdb:
sudo gdb ./build/flow_table -ex "run -c 10 -n 4 --proc-type=secondary -- -n 0"

```
