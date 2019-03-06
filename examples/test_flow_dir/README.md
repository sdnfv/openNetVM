Flow Director Test
==

This NF demonstrates how to use ONVM's Flow Director. When a packet arrives the NF checks whether it is from a flow that already has a service chain rule. If not, it creates a new rule so the packet will be sent to the destination NF indicated on the command line. Packets that match a rule are processed with the ONVM_NF_ACTION_NEXT action.

Compilation and Execution
--
```
cd examples
make
cd test_flow_dir
./go.sh SERVICE_ID -d DST [PRINT_DELAY]

OR

./go.sh -F CONFIG_FILE -- -- -d DST [-p PRINT_DELAY]

OR

sudo ./test_flow_dir/x86_64-native-linuxapp-gcc/forward -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- -d DST [-p PRINT_DELAY]
```

App Specific Arguments
--
  - `-d <dst>`: destination service ID to foward to
  - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.

Config File Support
--
This NF supports the NF generating arguments from a config file. For
additional reading, see [Examples.md](../../docs/Examples.md)

See `../example_config.json` for all possible options that can be set.

