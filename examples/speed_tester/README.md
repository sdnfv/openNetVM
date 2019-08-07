TX Speed Tester NF
==
This program lets you test how fast the manager's TX threads can move packets. It works by creating a batch of packets and then repeatedly sending that batch of packets to itself or another NF.

Optional Libraries
--
libpcap library is required for pcap replay functionality.
To enable pcap replay set `ENABLE_PCAP=1` in the Makefile.
If libpcap is not installed run:
```
sudo apt-get install libpcap-dev
```
Example .pcap files
--
`pcap/` directory has a few example pcap files.
  - pktgen_big.pcap, pktgen_large.pcap, pktgen_test1.pcap, pktgen_traffic_sample.pcap are taken from the [Pktgen](../../tools/Pktgen/README.md) example pcap files.
  - 64B_download.pcap, 8K_download.pcap are sample web traffic pcap files.

Compilation and Execution
--
```
cd examples
make
cd speed_tester
./go.sh -d DST_ID [PRINT_DELAY] [ADVANCED_RINGS] [PACKET_SIZE] [DEST_MAC] [PCAP_FILE] [MEASURE_LATENCY]

OR

./go.sh -F CONFIG_FILE -- -- -d DST_ID [-p PRINT_DELAY] [-s PACKET_SIZE] [-m DEST_MAC] [-o PCAP_FILE] [-l MEASURE_LATENCY]

OR

sudo ./build/speed_tester -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- -d DST [-p PRINT_DELAY] [-s PACKET_SIZE] [-m DEST_MAC] [-o PCAP_FILENAME] [-l]
```

App Specific Arguments
--
  - `-d DST`: Destination Service ID to foward to
  - `-p PRINT_DELAY`: Number of packets between each print, e.g. `-p 1` prints every packets.
  - `-s PACKET_SIZE`: Size of packet, e.g. `-s 32` allocates 32 bytes for the data segment of `rte_mbuf`.
  - `-m DEST_MAC`: User specified destination MAC address, e.g. `-m aa:bb:cc:dd:ee:ff` sets the destination address within the ethernet header that is located at the start of the packet data.
  - `-o PCAP_FILENAME` : The filename of the pcap file to replay
  - `-l LATENCY` : Enable latency measurement. This should only be enabled on one Speed Tester NF. Packets must be routed back to the same speed tester NF.
  - `-c PACKET_NUMBER` : Use user specified number of packets in the batch. If not specified then this defaults to 128.

Config File Support
--
This NF supports the NF generating arguments from a config file. For
additional reading, see [Examples.md](../../docs/Examples.md)

See `../example_config.json` for all possible options that can be set.
