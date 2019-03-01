Flow Tracker
==
Flow Tracker is an NF that stores and displays information about incoming flows and
then sends them to another NF. The print frequency can be customized
with a command-line argument.

Compilation and Execution
--
```
cd examples
make
cd flow_tracker
./go.sh SERVICE_ID -d DESTINATION_ID [-p PRINT_DELAY]

OR

./go.sh -F CONFIG_FILE -- -- -d DESTINATION_ID [-p PRINT_DELAY]

OR

sudo ./build/flow_tracker -l CORELIST -n NUM_MEMORY_CHANNELS --proc-type=secondary -- -r SERVICE_ID -- -d DESTINATION_ID [-p PRINT_DELAY]
```

App Specific Arguments
--
  - `-d <destination_id>`: Service ID to send packets to, e.g. `-d 2`
    sends packets to the NF using service ID 2
  - `-p <print_delay>`:  Number of seconds between each print (default
    is 5)

Config File Support
--
This NF supports the NF generating arguments from a config file. For
additional reading, see [Examples.md](../../docs/Examples.md)

See `../example_config.json` for all possible options that can be set.
