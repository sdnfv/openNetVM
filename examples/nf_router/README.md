NF Router
==
Example NF that routes packets to NFs based on the provided rules.
The NF will compare the incoming packet dest_ip with the IPs from the config file and decide which NF to send the packet to. If a match isn't found the packet is dropped.

App Specific Instructions
--
This NF requires a router config file. The config file must have a list of tuples in the form of IP dest_id, an example config file is provided in `route.conf`.

Compilation and Execution
--
```
cd examples
make
cd nf_router
./go.sh SERVICE_ID -f ROUTER_CONFIG [PRINT_DELAY]

OR

./go.sh -F CONFIG_FILE -- -- -f ROUTER_CONFIG [-p PRINT_DELAY]

OR

sudo ./build/nf_router -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- -f ROUTER_CONFIG [-p PRINT_DELAY]
```

App Specific Arguments
--
  - `-f <router_cfg>`: router configuration, has a list of (IPs, dest) tuples 
  - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.

Config File Support
--
This NF supports the NF generating arguments from a config file. For
additional reading, see [Examples.md](../../docs/Examples.md)

See `../example_config.json` for all possible options that can be set.

