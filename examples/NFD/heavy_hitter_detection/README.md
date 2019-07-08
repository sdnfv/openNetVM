HeavyHitterDetection
==
Heavy Hitter Detection developed by the NFD framework


NFD is a NF developing framework, consisting two parts, NFD language and NFD compiler. NFD compiler translate <br>
the NF model file wiiten in NFD language, a table-form language, into standard runtime environment(such as C++).<br><br>


Heavy Hitter Detection is translated from the `HHDmodel.txt` to C++ environment. <br>
 
 <br>
 
Heavy Hitter Detection keeps a counter for per flow and detects which flows consume most bandwidth by comparing the counters with a threshold. (threshold is set by user, in this program, we set it 100.) If the count reaches threshold, we will drop the following packets.


Testing
--

The Heavy Hitter Detection NF will start dropping SYN packets(from the same source IP address) after a certain threshold is reached, to verify this run these 2 NFs:

Run the Heavy Hitter Detection NF with:

```
./go.sh 1 -d 2

```

Run Speed Tester NF(to replay pcap file) with:

```
./go.sh 2 -d 1  -o pcap/64B_download.pcap  

```


Compilation and Execution
--

To run this NF, you should use either clang++ or g++ to compile this NF developed by C++.

```
cd heavy_hitter_detection
make

```

Then, you can run this code.

```
./go.sh CORELIST SERVICE_ID DST [PRINT_DELAY]

OR

./go.sh -F CONFIG_FILE -- -- -d DST [-p PRINT_DELAY]

OR

sudo ./build/heavy_hitter_detection -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- -d DST [-p PRINT_DELAY]
```

App Specific Arguments
--
  - `-d <dst>`: destination service ID to foward to
  - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.

Config File Support
--
This NF supports the NF generating arguments from a config file. For additional reading, see [Examples.md](../../docs/Examples.md)

See `../example_config.json` for all possible options that can be set.
