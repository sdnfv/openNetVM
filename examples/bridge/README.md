Bridge
==
This is an example NF that acts as a basic bridge - it sends packets from one port to the other.

Compilation and Execution
--
```
cd examples
make
cd bridge
./go.sh CORELIST SERVICE_ID [PRINT_DELAY]

OR

sudo ./build/bridge -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- [-p PRINT_DELAY]
```

App Specific Arguments
--
  - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.
