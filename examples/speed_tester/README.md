TX Speed Tester NF
==
This program lets you test how fast the manager's TX threads can move packets. It works by creating a batch of packets and then repeatedly sending that batch of packets to itself or another NF.

Compilation and Execution
--
```
cd examples
make
cd speed_tester
./go.sh CORELIST SERVICE_ID DST_ID [PRINT_DELAY] [ADVANCED_RINGS] [PACKET_SIZE] [DEST_MAC]

OR

sudo ./build/speed_tester -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- -d DST [-a] [-p PRINT_DELAY] [-s PACKET_SIZE] [-m DEST_MAC]
```

App Specific Arguments
--
  - `-d DST`: Destination Service ID to foward to
  - `-a`: Use advanced rings interface instead of default `packet_handler`
  - `-p PRINT_DELAY`: Number of packets between each print, e.g. `-p 1` prints every packets.
  - `-s PACKET_SIZE`: Size of packet, e.g. `-s 32` allocates 32 bytes for the data segment of `rte_mbuf`.
  - `-m DEST_MAC`: User specified destination MAC address, e.g. `-m aa:bb:cc:dd:ee:ff` sets the destination address within the ethernet header that is located at the start of the packet data.  
