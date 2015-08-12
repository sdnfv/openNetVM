Pktgen Installation (DPDK-1.8.0 Version)
===================




#### Welcome to installation memo for [Pktgen](http://pktgen.readthedocs.org/en/latest/index.html), Pktgen is an app build on [DPDK](http://dpdk.org/),  which is a high performance traffic generator. 

This guide is assuming that you have already got openNetVM installed on your machine, if not, please follow installation guide for openNetVM in file ***INSTALL.md***. 

----------


1. Preparation Steps
===================

1.1 check if you have free hugepages

`$grep -i huge /proc/meminfo`

If ***HugePages_Free*** 's value equals to 0, which means there is no free hugepages available, you probably have to reboot your machine, by `$sudo reboot` to get some released hugepages. After rebooting your computer please refer to installation guide for openNetVM step ***5*** for guiance of creating Hugepage directory and reserving memory.

```
AnonHugePages:      2048 kB
HugePages_Total:    1024
HugePages_Free:       64
HugePages_Rsvd:        0
HugePages_Surp:        0
Hugepagesize:       2048 kB
```

1.2 check your current status of NIC binding and active status, if your 10G ports are binded with DPDK driver, you are fine, if not, please refer to installation guide openNetVM for instructions for unbinding NIC cards from kernal uio and rebinding to DPDK.  

`$.../your_dpdk_directory/tools/dpdk_nic_bind.py  --status`

```
Network devices using DPDK-compatible driver
============================================
0000:07:00.0 '82599EB 10-Gigabit SFI/SFP+ Network Connection' drv=igb_uio unused=ixgbe
0000:07:00.1 '82599EB 10-Gigabit SFI/SFP+ Network Connection' drv=ixgbe unused=igb_uio

Network devices using kernel driver
===================================
0000:05:00.0 '82576 Gigabit Network Connection' if=eth0 drv=igb unused=igb_uio *Active*
```



2. Installation of Pktgen 
===================


2.1 Get Package 
-------------

2.1.1  entering working directory and download	latest source	code

`$cd /home/**your_name**/openNetVM/`

`$git clone http://dpdk.org/git/apps/pktgen-dpdk`



2.2 Build Pktgen Application
------------- 

2.2.1 enter working directory, and compile the application

`$cd ./pktgen-dpdk/`

`$make`


2.2.2 executing the example

test the pktgen by:

`$sudo ./pktgen -c 3 -n 1`

2.2.3 updating configuration for pktgen, three servers are set up for observing the traffic flow: web client ----> port 0 - ONVM - port 1----> web server

if the test run goes through, please set up example configuration file as below:

`$sudo vim doit.sh`

```
***comment all lines and add the following scripts in the doit.sh file***
echo "start"

./app/build/pktgen -c ffff -n 3 $BLACK_LIST -- -p 0x3 -P -m "[4:8].0" -f  /home/***your_name***/openNetVM/pktgen-dpdk/forward_3_server.lua

echo "done"
```

`$sudo vim forward_3_server.lua`

```
***please add these following  lines in your forward_3_server.lua file***

--package.path = package.path ..";?.lua;test/?.lua;app/?.lua;"

-- A list of the test script for Pktgen and Lua.
-- Each command somewhat mirrors the pktgen command line versions.
-- A couple of the arguments have be changed to be more like the others.
--

printf("Lua Version      : %s\n", pktgen.info.Lua_Version);
printf("Pktgen Version   : %s\n", pktgen.info.Pktgen_Version);
printf("Pktgen Copyright : %s\n", pktgen.info.Pktgen_Copyright);

prints("pktgen.info", pktgen.info);

printf("Port Count %d\n", pktgen.portCount());
printf("Total port Count %d\n", pktgen.totalPorts());


-- set up a mac address to set flow to 
--  
-- TO DO LIST:
--
-- please update this part with the destination mac address, source and destination ip address you would like to sent packets to 

pktgen.set_mac("0", "1c:c1:de:f0:e6:0a"); 
pktgen.set_ipaddr("0", "dst", "10.11.1.17");
pktgen.set_ipaddr("0", "src", "10.11.1.16");



pktgen.set_proto("all", "udp");
pktgen.set_type("all", "ipv4");

pktgen.set("all", "size", 64)
pktgen.set("all", "burst", 32);
pktgen.set("all", "sport", 1234);
pktgen.set("all", "dport", 1234);
pktgen.set("all", "count", 100000);
pktgen.set("all", "rate",100);

pktgen.vlan_id("all", 5);
```

2.3 executing the example
------------- 

`$sudo bash doit.sh`

if you got your result as below, , then you are all set
```


#     #
#  #  #     #    #    #  #####
#  #  #     #    ##   #  #    #
#  #  #     #    # #  #  #    #
#  #  #     #    #  # #  #    #
#  #  #     #    #   ##  #    #
## ##      #    #    #  #####

######
#     #     #    #    #  ######  #####
#     #     #    #    #  #       #    #
######      #    #    #  #####   #    #
#   #       #    #    #  #       #####
#    #      #     #  #   #       #   #
#     #     #      ##    ######  #    #

#####
#     #   #   #   ####    #####  ######  #    #   ####
#          # #   #          #    #       ##  ##  #
#####      #     ####      #    #####   # ## #   ####
#     #         #     #    #       #    #       #
#     #     #    #    #     #    #       #    #  #    #
#####      #     ####      #    ######  #    #   ####

Copyright (c) <2010-2014>, Wind River Systems, Inc.
>>> Pktgen is Powered by IntelÂ® DPDK <<<

---------------------

Copyright (c) <2010-2014>, Wind River Systems, Inc.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

1) Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2) Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

3) Neither the name of Wind River Systems nor the names of its contributors may be
used to endorse or promote products derived from this software without specific
prior written permission.

4) The screens displayed by the application must contain the copyright notice as defined
above and can not be removed without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Pktgen created by Keith Wiles -- >>> Powered by IntelÂ® DPDK <<<
- Ports 0-3 of 6   ** Main Page **  Copyright (c) <2010-2014>, Wind River Systems, Inc. Powered by IntelÂ® DPDK
Flags:Port    :   P-------------:0   P-------------:1
Link State      :      <UP-10000-FD>      <UP-10000-FD>                                          ---TotalRate---
Pkts/s  Rx      :                  0                  0                                                        0
Tx      :                  0                  0                                                        0
MBits/s Rx/Tx   :                0/0                0/0                                                      0/0
Broadcast       :                  0                  0
Multicast       :                  0                  0
64 Bytes      :                  0                  0
65-127        :                  0                  0
128-255       :                  0                  0
256-511       :                  0                  0
512-1023      :                  0                  0
1024-1518     :                  0                  0
Runts/Jumbos    :                0/0                0/0
Errors Rx/Tx    :                0/0                0/0
Total Rx Pkts   :                  0                  0
Tx Pkts   :                  0                  0
Rx MBs    :                  0                  0
Tx MBs    :                  0                  0
ARP/ICMP Pkts   :                0/0                0/0
:
Tx Count/% Rate :       Forever/100%       Forever/100%
PktSize/Tx Burst:              64/16              64/16
Src/Dest Port   :          1234/5678          1234/5678
Pkt Type:VLAN ID:      IPv4/TCP:0001      IPv4/TCP:0001
Dst  IP Address :        192.168.1.1        192.168.0.1
Src  IP Address :     192.168.0.1/24     192.168.1.1/24
Dst MAC Address :  90:e2:ba:5a:f7:91  90:e2:ba:5a:f7:90
Src MAC Address :  90:e2:ba:5a:f7:90  90:e2:ba:5a:f7:91
-- Pktgen Ver:2.7.1(DPDK-1.7.0) -------------------------------------------------------------------------------------

```

Please use `pktgen> quit` for existing. 









