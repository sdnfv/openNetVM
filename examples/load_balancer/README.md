Load Balancer NF
==

This NF acts as a layer 3, round-robin load balancer. When a packet arrives the NF checks whether it is from an already existing flow. If not, it creates a new flow entry and assigns it to the destination backend server. The NF decides what to do with the packet based on which port it arrived at. It is also setup to clean the flow table whenever it fills up.

App Specific Instuctions
--
**Setting up dpdk interfaces**  
This NF requires 2 DPDK interfaces to work, both can be setup using the `openNetVM/dpdk/tools/dpdk-setup-iface.sh` script.  

**Server Config**  
The server config needs to have the total number of backend servers with their ip and mac address combination, an example config file `server.conf` is provided.  

**Server Configuration**  
The backend servers need to be configured to forward traffic back to the load balancer, this can be done using ip routes on the server machine.  
An example usage for LB server port at 10.0.0.37 with clients matching 11.0.0.0/24 using iface p2p1.   
```sudo ip route add 11.0.0.0/24 via 10.0.0.37 dev p2p1```  

**This NF should be run with the ARP NF**    
The Load Balancer NF needs to respond to client and server ARP requests. As `onvm_mgr` currently doesn't resolve arp requests, an ARP NF    with the LB NF as destination is used.

An example usage of the ARP&LB NF, with the Load Balancer using dpdk0 - 10.0.0.37 and  dpdk1 - 11.0.0.37 for client, server ports respecively. 
```
ARP NF
./go.sh 4 1 2 -s 10.0.0.37,11.0.0.37

LB NF
./go.sh 5 2 dpdk0 dpdk1 server.conf 
```


Compilation and Execution
--
```
make
./go.sh CORELIST SERVICE_ID CLIENT_IFACE SERVER_IFACE SERVER_CONFIG [PRINT_DELAY]

OR

sudo ./load_balancer/x86_64-native-linuxapp-gcc/forward -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- -c CLIENT_IFACE -s SERVER_IFACE -f SERVER_CONFIG [-p PRINT_DELAY]
```

App Specific Arguments
--
  - `CLIENT_IFACE` : name of the client interface
  - `SERVER_IFACE` : name of the server interface
  - `SERVER_CONFIG` : backend server config file
  - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.
