Simple Forward
==
Example NF that forwards packets to a specific destination.

Compilation and Execution
--
```
cd examples
make
cd simple_forward
./go.sh CORELIST SERVICE_ID DST [PRINT_DELAY]

OR

sudo ./build/forward -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- -d DST [-p PRINT_DELAY]
```

App Specific Arguments
--
  - `-d <dst>`: destination service ID to foward to
  - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.
