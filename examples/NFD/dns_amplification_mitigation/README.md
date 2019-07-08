DNSAmplificationMitigation
==
DNS Amplification Mitigation developed by the NFD framework


NFD is a NF developing framework, consisting two parts, NFD language and NFD compiler. NFD compiler translate <br>
the NF model file wiiten in NFD language, a table-form language, into standard runtime environment(such as C++).<br><br>


DNS Amplification Mitigation is translated from the `DNSAmplificationMitigationModel.txt` to C++ environment. <br>
 
 
 <br>
This NF is designed to be deployed in the user's end. An attacker uses a spoofed server IP to request many DNS queries that result in large answers to that DoS the server with the spoofed IP. Mitigation is by tracking if the server actually committed this request. In this program, we use a map to track all the real DNS requests from the server with the spoofed IP and pass the real DNS responses while block the responces due to attacker. 


Testing
--

The DNS Amplification mitigation NF will drop all DNS response packets(TCP source port is 53) whose source IP is the user never send a DNS request to. To verify this run these 2 NFs:

Run the DNS Amplification mitigation NF with:

```
./go.sh 1 -d 2

```

Run Speed Tester NF(to replay pcap file) with:

```
./go.sh 2 -d 1  -o pcap/pktgen_large.pcap 

```
Above command will not trigger the dropping process.

In order to trigger the dropping process, we need to filter the packets whose source port is 53 only(in wireshark apply `dns and dns and udp.srcport==53` filter the packets) from pktgen_large.pcap.

And run:
```
./go.sh 2 -d 1  -o pcap/udpResponseOnly.pcap 

```


Compilation and Executionthe 
--

To run this NF, you should use either clang++ or g++ to compile this NF developed by C++.

```
cd dns_amplification_mitigation
make

```

Then, you can run this code.

```
./go.sh CORELIST SERVICE_ID DST [PRINT_DELAY]

OR

./go.sh -F CONFIG_FILE -- -- -d DST [-p PRINT_DELAY]

OR

sudo ./build/dns_amplification_mitigation -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- -d DST [-p PRINT_DELAY]
```

App Specific Arguments
--
  - `-d <dst>`: destination service ID to foward to
  - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.

Config File Support
--
This NF supports the NF generating arguments from a config file. For additional reading, see [Examples.md](../../docs/Examples.md)

See `../example_config.json` for all possible options that can be set.
