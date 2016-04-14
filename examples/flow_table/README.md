Simple Flow Table
==

This NF demonstrates how flow_table follow rules set by SDN controller, and send packets to their destination based on the packet 5-tuple and the port (physical or NF) of where the packet came from and the rules set by SDN controller.

When a packet arrives,  the NF checks whether it is from a flow that already has a service chain rule. If not, it contacts SDN controller for a rule to follow, so the packet will be sent to the destination NF. Packets that match a rule are processed with the ONVM_NF_ACTION_NEXT action.

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
