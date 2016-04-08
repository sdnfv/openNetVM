TX Speed Tester NF
==
This program lets you test how fast the manager's TX threads can move packets. It works by creating a batch of packets and then repeatedly sending that batch of packets to itself or another NF.

Compilation and Execution
--
```
cd examples
make
cd speed_tester
./go.sh CORELIST SERVICE_ID DST_ID [PRINT_DELAY]

OR

sudo ./build/speed_tester -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- -d DST [-p PRINT_DELAY]
```

App Specific Arguments
--
  - `-d DST`: Destination Service ID to foward to
  - `-p PRINT_DELAY`: Number of packets between each print, e.g. `-p 1` prints every packets.
