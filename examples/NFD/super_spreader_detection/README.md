Super Spreader Detection
==
Super Spreader Detection developed by the NFD framework


NFD is a NF developing framework, consisting two parts, NFD language and NFD compiler. NFD compiler translate <br>
the NF model file wiiten in NFD language, a table-form language, into standard runtime environment(such as C++).<br><br>


Super Spreader Detection is translated from the `SSDmodel.txt` to C++ environment. <br>
  
 <br>
 Super Spreader Detection detects and identifies super spreaders to preempt port scan attacks or DDoS attacks by increasing the counter on SYNs and decrease it on FINs. The count will be compared to threshold set by user. In this program, we set threshold 100.
 <br>
 

Testing
--

The Super Spreader Detection NF will drop the flow packets(denoted by source IP) whose accumulated SYN num (SYN packets from a source packet will be accumulated the number while the FIN packets from same source will decrease the number) reaches to threshold. To verify the dropping process, you should filter all SYN packets out without its corresponding FIN packets. Then you can trigger the dropping process. Run these 2 NFs:

Run the Super Spreader Detection NF with:

```
./go.sh 1 -d 2

```

Run Speed Tester NF(to replay pcap file) with:

```
./go.sh 2 -d 1  -o pcap/SYNWithoutFIN.pcap 

```



Compilation and Execution
--

To run this NF, you should use either clang++ or g++ to compile this NF developed by C++.

```
cd super_spreader_detection
make

```

Then, you can run this code.

```
./go.sh CORELIST SERVICE_ID DST [PRINT_DELAY]

OR

./go.sh -F CONFIG_FILE -- -- -d DST [-p PRINT_DELAY]

OR

sudo ./build/super_spreader_detection -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- -d DST [-p PRINT_DELAY]
```

App Specific Arguments
--
  - `-d <dst>`: destination service ID to foward to
  - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.

Config File Support
--
This NF supports the NF generating arguments from a config file. For additional reading, see [Examples.md](../../docs/Examples.md)

See `../example_config.json` for all possible options that can be set.
