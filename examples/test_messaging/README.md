Test Messaging
==
This is an example NF that acts as a unit test for sending messsages from an NF to itself.

Compilation and Execution
--
```
cd examples
make
cd test_messaging
./go.sh SERVICE_ID [PRINT_DELAY]
```

App Specific Arguments
--
  - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packet.

Config File Support
--
This NF supports the NF generating arguments from a config file. For
additional reading, see [Examples.md](../../docs/Examples.md)

See `../example_config.json` for all possible options that can be set.