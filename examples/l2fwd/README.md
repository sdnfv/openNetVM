Layer 2 Switch
==
l2switch is an NF based on the dpdk [l2fwd example](https://doc.dpdk.org/guides/sample_app_ug/l2_forward_real_virtual.html) that sends packets out the adjacent port. The destination port is the adjacent port from the enabled portmask, that is, if the first four ports are enabled (portmask 0xf),
ports 1 and 2 forward into each other, and ports 3 and 4 forward into each other. Individual packets destination MAC address is replaced by 02:00:00:00:00:TX_PORT_ID.

Compilation and Execution
--
```
cd examples
make
cd l2switch
./go.sh SERVICE_ID [PRINT_DELAY]

OR

./go.sh -F CONFIG_FILE -- -- [-p PRINT_DELAY]

OR

sudo ./build/bridge -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- [-p PRINT_DELAY]
```

App Specific Arguments
--
  - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets. Default is every 1000000 packets.
  - `-n` : Disables mac updating.
  - `-m` : Enables printing updated mac address. Prints the updated mac address of every packet.

For example: ./go.sh 1 -p 1 -m

Will print every packet with mac address information.

Config File Support
--
This NF supports the NF generating arguments from a config file. For
additional reading, see [Examples.md](../../docs/Examples.md)

See `../example_config.json` for all possible options that can be set.
