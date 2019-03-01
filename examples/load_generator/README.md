Load Generator
==
This NF generates and sends packets with defined rates and sizes, and it measures latency when its packets are returned to itself.

Compilation and Execution
--
```
cd examples
make
cd load_generator
./go.sh SERVICE_ID -d DST_ID [-p PRINT_DELAY] [-t PACKET_RATE] [-m DEST_MAC] [-s PACKET_SIZE] [-o]

OR

sudo ./build/load_generator -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- -d DST [-p PRINT_DELAY] [-t PACKET_RATE] [-m DEST_MAC] [-s PACKET_SIZE] [-o]
```

App Specific Arguments
--
  - `-d <dst>`: destination service ID to forward to, or dst port if `-o` is used.
  - `-p <print_delay>`: number of seconds between each print (e.g. `-p 0.1` prints every 0.1 seconds).
  - `-t <packet_rate>`: the desired transmission rate for the packets (e.g. `-t 3000000 transmits 3 million packets per second). Note that the actual transmission rate may be limited based on system performance and NF configuration. If the load generator is experiencing high levels of dropped packets either transmitting or receiving, lowering the transmission rate could solve this.
  - `-m <dest_mac>`: user specified destination MAC address (e.g. `-m aa:bb:cc:dd:ee:ff` sets the destination address within the ethernet header that is located at the start of the packet data).
  - `-s <packet_size>`: the desired size of the generated packets in bytes.
  - `-o`: send the packets out the NIC port.

