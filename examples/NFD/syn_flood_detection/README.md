SYNFloodDetection
==
SYN Flood Detection developed by the NFD framework


NFD is a NF developing framework, consisting two parts, NFD language and NFD compiler. NFD compiler translate <br>
the NF model file wiiten in NFD language, a table-form language, into standard runtime environment(such as C++).<br><br>


SYN Flood Detection is translated from the `SYNFloodDetectionModel.txt` to C++ environment. <br>
 
 <br>
 SYN Flood Detection counts the number of SYNs without any matching ACK from the sender side. If one sender exceeds a certain threshold X (set by user. In this program, we set threshold 100.), it should be blocked.
 
 <br>



Testing
--

The SYN Flood Detection NF counts the number of SYNs without any matching ACK from the sender side. If one sender(denoted by IP) exceeds a certain threshold X (set by user. In this program, we set threshold 100.), it should be blocked. To trigger the dropping process, you need to just send the SYN packets without ACK (from sender to server) and accumulate it to the threshold. You can filter from the `pktgen_large.pcap` by `tcp.flags.syn==1 and tcp.flags.ack!=1` to get the test trace. Run these 2 NFs:

Run the SYN Flood Detection NF with:

```
./go.sh 1 -d 2

```

Run Speed Tester NF(to replay pcap file) with:

```
./go.sh 2 -d 1  -o pcap/SYNWithoutACK.pcap 

```
 



Compilation and Execution
--

To run this NF, you should use either clang++ or g++ to compile this NF developed by C++.

```
cd syn_flood_detection
make

```

Then, you can run this code.

```
./go.sh CORELIST SERVICE_ID DST [PRINT_DELAY]

OR

./go.sh -F CONFIG_FILE -- -- -d DST [-p PRINT_DELAY]

OR

sudo ./build/syn_flood_detection -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- -d DST [-p PRINT_DELAY]
```

App Specific Arguments
--
  - `-d <dst>`: destination service ID to foward to
  - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.

Config File Support
--
This NF supports the NF generating arguments from a config file. For additional reading, see [Examples.md](../../docs/Examples.md)

See `../example_config.json` for all possible options that can be set.
