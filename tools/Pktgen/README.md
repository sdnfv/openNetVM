Pktgen Installation 
===================




#### Welcome to installation memo for [Pktgen](http://pktgen.readthedocs.org/en/latest/index.html), Pktgen is an app build on [DPDK](http://dpdk.org/),  which is a high performance traffic generator. 

This guide is assuming that you have already got openNetVM installed on your machine, if not, please follow installation guide for openNetVM in file ***INSTALL.md***. 

----------


1 Preparation Steps  
===================

1.1 Check if you have free hugepages
------------- 

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

1.2 Check your current status of NIC binding and active status
------------- 

If your 10G ports are binded with DPDK driver, you are fine, if not, please refer to installation guide openNetVM for instructions for unbinding NIC cards from kernal uio and rebinding to DPDK.  


```
Network devices using DPDK-compatible driver
============================================
0000:07:00.0 '82599EB 10-Gigabit SFI/SFP+ Network Connection' drv=igb_uio unused=ixgbe
0000:07:00.1 '82599EB 10-Gigabit SFI/SFP+ Network Connection' drv=ixgbe unused=igb_uio

Network devices using kernel driver
===================================
0000:05:00.0 '82576 Gigabit Network Connection' if=eth0 drv=igb unused=igb_uio *Active*
```



2 Installation of Pktgen 
===================


2.1 Get Package 
-------------

Initialize pktgen submodule

`git submodule init && git submodule update`

Install pcap dependency

`sudo apt-get install libpcap-dev`

Download Lua, can be found [here](https://www.lua.org/download.html)  
This might require running:  
`sudo apt-get install libreadline-dev`  
Navigate into the downloaded Lua directory and run  
`sudo make install`

2.2 Build Pktgen Application
------------- 

Enter working directory, and compile the application

`$cd tools/Pktgen/pktgen-dpdk/`

`$make`

Test pktgen by:

`$sudo ./app/x86_64-native-linuxapp-gcc/pktgen -c 3 -n 1`

Updating configuration for pktgen, three servers are set up for observing the traffic flow: web client ----> port 0 - ONVM - port 1----> web server


2.3 Configure pktgen for openNetVM
------------- 

Script files are located in openNetVM-Scripts

1. Modify mac and optionally src/dest ip in the `pktgen-config.lua`.

2.4 Run pktgen
------------- 

```sh
./openNetVM-Scripts/run-pktgen.sh 1
```   

If you got your result as below, then you are all set
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

Run `start all` to start sending packets.  

Please use `pktgen> quit` for existing. 

Licensing
-------------

```
BSD LICENSE

Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
Copyright(c) 2015 George Washington University
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in
the documentation and/or other materials provided with the
distribution.
The name of the author may not be used to endorse or promote
products derived from this software without specific prior
written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```
