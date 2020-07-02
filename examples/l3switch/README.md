Layer 3 Switch
==
l3switch is based on DPDK's [l3fwd example](https://doc.dpdk.org/guides/sample_app_ug/l3_forward.html) that leverages the openNetVM's flow director API. This NF has two modes, exact match and longest prefix match. In longest prefix match mode, a lookup matches a packet to a destination port. The lookup is based on the destination IP address. In exact match mode, a lookup is is based on the packet 5-tuple i.e. destination and src of the packets port and IP.

Hash entry number refers to the number of flow rules when running in exact match mode.

Compilation and Execution
--
```
cd examples
make
cd l3switch
./go.sh SERVICE_ID [PRINT_DELAY]

OR

./go.sh -F CONFIG_FILE -- -- [-p PRINT_DELAY]

OR

sudo ./build/l3switch -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- [-p PRINT_DELAY]
```

App Specific Arguments
--
  -p <print_delay>: number of packets between each print, e.g. `-p 1` prints every packets. Default is every 1000000 packets.
  -e : Enables exact match mode.
  -h <hash entry number> : Sets the hash entry number.

For example: ./go.sh 1 -e -h 7

Will enable exact match mode with hash entry number set to 7. Hash entry number will default to 4 if not manually set or not running in exact match mode.

Config File Support
--
This NF supports the NF generating arguments from a config file. For
additional reading, see [Examples.md](../../docs/Examples.md)

See `../example_config.json` for all possible options that can be set.
