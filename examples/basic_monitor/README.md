Basic Monitor
==
Basic Monitor is an NF which prints information regarding incoming packets and then sends it out of the system.

Compilation and Execution
--
```
cd examples
make
cd basic_monitor
./go.sh CORELIST SERVICE_ID [PRINT_DELAY]

OR

sudo ./build/monitor -l CORELIST -n NUM_MEMORY_CHANNELS --proc-type=secondary -- -r SERVICE_ID -- [-p PRINT_DELAY]
```

App Specific Arguments
--
  - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.
