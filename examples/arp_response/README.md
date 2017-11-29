ARP Response
==
ARP Response is an NF that responds to ARP Requests. The NF allows the user to set the IP address of each NIC port that needs to send ARP replies. The IPs are passed in as a comma separated list, `<IP for first port>,<IP for second port>,etc`, with each port gettting mapped to the corresponding port.

Compilation and Execution
--
```
make
./go.sh CORELIST SERVICE_ID DESTINATION_ID -s SOURCE_IP_LIST [-p enable
print]

OR

sudo ./build/monitor -l CORELIST -n NUM_MEMORY_CHANNELS --proc-type=secondary -- -r SERVICE_ID -- -d DESTINATION_ID -s SOURCE_IP_LIST [-p ENABLE PRINT]
```

App Specific Arguments
--
  - `-d <destination_id>`: the NF will send non-ARP packets to the NF at this service ID, e.g. `-d 2` sends packets to service ID 2
  - `-s <source_ip_list>`: the NF will map each comma separated IP (no spaces) to the corresponding port. Example: `-s 10.0.0.31,11.0.0.31` maps port 0 to 10.0.0.31, and port 1 to 11.0.0.31. If 0.0.0.0 is inputted, the IP will be 0. If too few IPs are inputted, the remaining ports will be ignored.
  - `-p`: Enables printing of log information
