Stateless Firewall
==
Stateless Firewall developed by the NFD framework


NFD is a NF developing framework, consisting two parts, NFD language and NFD compiler. NFD compiler translate <br>
the NF model file wiiten in NFD language, a table-form language, into standard runtime environment(such as C++).<br><br>


Stateless firewall is translated from the `model.txt` to C++ environment and describes the following table-form logic. <br>
  

**Match flow**      |**Match state**     | **Action flow**     | **Action state**            
 --------- | -----------  | ----------- |----------
 allow | * | pass | -
 not allow | * | drop | -

 
 <br>
 Stateless Firewall allows connections satisfying the whitelist or blocks those hitting the blacklist.

<br>
 

Testing
--

To test stateless firewall NF functionality, we need some traces which have packets with source IP of `192.168.22.0/24` or just modify the ALLOW(`line 133: IP _t1()` in source code) into your ALLOW nwetwork. It wiil drop all packets not belong to ALLOW. Run the stateless firewall NF:

```
./go.sh 1 -d 2

```

Run Speed Tester NF(to replay pcap file) with:

```
./go.sh 2 -d 1 -o pcap/trace.pcap 

```


Compilation and Execution
--

To run this NF, you should use either clang++ or g++ to compile this NF developed by C++.

```
cd stateless_firewall
make

```

Then, you can run this code.

```
./go.sh CORELIST SERVICE_ID DST [PRINT_DELAY]

OR

./go.sh -F CONFIG_FILE -- -- -d DST [-p PRINT_DELAY]

OR

sudo ./build/stateless_firewall -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- -d DST [-p PRINT_DELAY]
```

App Specific Arguments
--
  - `-d <dst>`: destination service ID to foward to
  - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.

Config File Support
--
This NF supports the NF generating arguments from a config file. For additional reading, see [Examples.md](../../docs/Examples.md)

See `../example_config.json` for all possible options that can be set.
