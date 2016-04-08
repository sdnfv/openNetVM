Simple Flow Table
==
This is a example with a simple flow table that matches a packet to a destination NF.  The lookup is based on the packet 5-tuple and the port (physical or NF) of where the packet came from.

Compilation and Exection
--
```
cd examples
make
cd flow_table
./go.sh CORELIST SERVICE_ID [PRINT_DELAY]

OR

sudo ./flow_table/x86_64-native-linuxapp-gcc/flow_table -l CORELIST -n NUM_MEMORY_CHANNELS --proc-type=secondary -- -r SERVICE_ID -- [-p PRINT_DELAY]
```

App Specific Arguments
--
  - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packet.
