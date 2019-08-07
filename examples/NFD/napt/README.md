NAPT
==
NAPT developed by the NFD framework


NFD is a NF developing framework, consisting two parts, NFD language and NFD compiler. NFD compiler translate <br>
the NF model file wiiten in NFD language, a table-form language, into standard runtime environment(such as C++).<br><br>


NAPT is translated from the `model.txt` to C++ environment. <br>
  
 <br>
 
NAPT is short for Network Address Port Translation. It enables mappings from tuples(address, L4 port number) to tuples(registered address and assigned port number) to complete address translation.
<br>
 

Testing
--

The NAPT NF will do the simple network address translation jobs. To verify it, first you need to generate two kinds of packets:1) srcIP is in the range(192.168.0.0/16 in the program); 2)dstIP is the NAT's IP (219.168.135.100 in program).

Run these 2 NFs:

Run the NAPT NF with:

```
./go.sh 1 -d 2

```

Run Speed Tester NF(to replay pcap file) with:

```
./go.sh 2 -d 1  -o pcap/trace.pcap 

```


Compilation and Execution
--

To run this NF, you should use either clang++ or g++ to compile this NF developed by C++.

```
cd examples/NFD/napt
make

```

Then, you can run this code.

```
./go.sh CORELIST SERVICE_ID DST [PRINT_DELAY]

OR

./go.sh -F CONFIG_FILE -- -- -d DST [-p PRINT_DELAY]

OR

sudo ./build/napt -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- -d DST [-p PRINT_DELAY]
```

App Specific Arguments
--
  - `-d <dst>`: destination service ID to foward to
  - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.

Config File Support
--
This NF supports the NF generating arguments from a config file. For additional reading, see [Examples.md](../../docs/Examples.md)

See `../example_config.json` for all possible options that can be set.
