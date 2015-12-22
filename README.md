openNetVM
===================
###A high performance container-based network function virtualization (NFV) platform.

This guide assumes that you are currently in **openNetVM** directory and have installed Intel DPDK Libraries and drivers and openNetVM itself.  See the [install guide](https://github.com/sdnfv/openNetVM/blob/master/docs/INSTALL.md) if you're not at this point.

openNetVM is a high-performance container based Network Function (NF) platform, which brings the ease of managing processes to middleboxes in networks such as firewalls, load balancers, and other such NFs.

This is a scalable platform where creating, destroying, and modifying NFs is very flexible.  Our platform has a manager which orchestrates traffic between the various NFs helping decrease traffic and performance degradation while making the three changes, creating, destroying, and modifying, to NFs.

Most NFs have rules to process packets based on their content.  This processing occurs on individual packets, but the applications are flooded with many interrupts and packets at once creating some bottlenecks in performance.  NFs such as load balancers, firewalls, proxies, etc. can achieve their goals if they processed individual packets at a time.  openNetVM allows this by providing a using a packet handler to request packets from the manager one-by-one. In this repository we provide three sample NFs, a monitoring NF, a simple forwarding NF, and a bridge NF to get you started.

----------

1. openNetVM Manager
-------------

The openNetVM Manager is responsible for managing traffic between all NFs in the network.  It handles all incoming RX to the system, all outgoing TX from the system, displays statistics regarding traffic between each NF, flags packets with the appropriate header information so they can get to their destination NF, and it handles providing packet requests from the NFs.

###1.1 onvm_mgr Manual

```
$sudo ./onvm_mgr/onvm_mgr/x86_64-native-linuxapp-gcc/onvm_mgr -c COREMASK -n NUM [-l CORELIST] [–lcores COREMAP] [-m MB] [-r NUM] [-v] [–syslog] [–socket-mem] [–huge-dir] [–proc-type] [ -- [-p PORTMASK] [-n NUM_CLIENTS] ]


Options:

	-c	an hexadecimal bit mask of the cores to run on.
		Note that core numbering can change between platforms and should be determined beforehand.

	-n	number of memory channels per processor socket.
		Note that here we usually set it up with 4.

	-l	list of cores to run on.
		Note that the format sample is as <c1>[-c2][,c3[-c4],...] , where c1, c2 ...  are core indexes between 0 and 128.

	–lcores	map lcore set to physical cpu set.
		Note that the format sample is as <lcores[@cpus]>[<,lcores[@cpus]>...]

	-m	memory to allocate.
       		Note that unit in MB.

	-r	set the number of memory ranks (auto-detected by default).

	-v	display the version information on startup.

	-syslog	set the syslog facility.

	-socket_mem	set the memory to allocate on specific sockets.
			Note that don't forget to use "," to seperate values.

	-huge-dir	directory where the hugetlbfs is mounted.

	–proc-type	set the type of the current process.
			Note that -proc-type=<auto|primary|secondary> is set as auto in this case.


	--	secondary commands

		-p	an hexadecimal bit mask of the ports to run on.
			Note that here we usually use Oxff

		-n	number of clients in total, here clients means network functions clients
			Note that the lexical indexing starts from 0, which means, if you set n as 2, clients indexes range from [0, 1]
```

2. Examples
-------------

###2.1 Simple Forward and Basic Monitor NF Chain

Simple Forward is an NF that forwards all incoming traffic to the next NF in the chain.  This NF should not be the last one in a chain of NFs.

The Basic Monitor NF displays information about the incoming packets and then sends them out back to where they came from.  This one should be the last NF in the chain.

####2.1.1 CPU Layout and Coremasks

First, you need to determine how many NFs your CPU architecture can handle.  To do so, please run the [./scripts/coremask.py](https://github.com/sdnfv/openNetVM/blob/master/scripts/coremask.py) script to determine this information.  On our machine, our output was:

```
#./scripts/coremask.py
You supplied 0 arguments, running with flag --onvm

===============================================================
                 openNetVM CPU Coremask Helper
===============================================================

openNetVM requires at least three cores for the manager.
After three cores, the manager can have more too.  Since
NFs need a thread for TX, the manager can have many dedicated
TX threads which would all need a dedicated core.

Each NF running on openNetVM needs its own core too.  One manager
TX thread can be used to handle two NFs, but any more becomes
inefficient.

Use the following information to run openNetVM on this system:

        - openNetVM Manager -- coremask: 0x7f

        - CPU Layout permits 7 NFs with these coremasks:
                + NF 0 -- coremask: 0x2000
                + NF 1 -- coremask: 0x1000
                + NF 2 -- coremask: 0x800
                + NF 3 -- coremask: 0x400
                + NF 4 -- coremask: 0x200
                + NF 5 -- coremask: 0x100
                + NF 6 -- coremask: 0x80
```

Based on this output, the manager should use the coremask 7f and we can run 7 NFs with the coremasks supplied.

####2.1.2 Run the openNetVM Manager

Use the CPU information from the previous step to run the manager:

`
$sudo ./onvm/onvm_mgr/onvm_mgr/x86_64-native-linuxapp-gcc/onvm_mgr -c 7f -n 4 -- -p 1 -n 7
`

####2.1.3 Run NFs Simple Forward and Basic Monitor

To run Simple Forward, launch 6 docker containers and execute the following command from them replacing the <COREMASK> portion with the respective coremask from the script ran in step 2.1.1 and the <NFID> portion with the appropriate NF ID 0-n:

`
$sudo ./examples/simple_forward/simple_forard/x86_64-native-linuxapp-gcc/forward -c <COREMASK> -n 4 --proc-type=auto -- -n <NFID>
`

Run basic monitor as the 7th NF in the chain in the last docker container using the last coremask provided and the last NF ID:

`
$sudo ./examples/basic_monitor/basic_monitor/x86_64-native-linuxapp-gcc/monitor -c <COREMASK> -n 4 --proc-type=auto -- -n <NFID>
`

Now, on another server, run Pktgen-DPDK or MoonGen (see documentation in [docs](https://github.com/sdnfv/openNetVM/tree/master/docs) directory), or another traffic generation technique of your choice.

###2.2 Bridge NF

Bridge is an application example build on openNetVM, which sends every packets from one port to another on the same node.

Please compile and build openNetVM first, and then execute openNetVM manager in one terminal window and run bridge application on another terminal window.

To run manager:

`
$sudo ./onvm_mgr/onvm_mgr/x86_64-native-linuxapp-gcc/onvm_mgr -c 6 -n 4 -- -p 1 -n 1
`

To run bridge:

`
$sudo ./examples/bridge/bridge/x86_64-native-linuxapp-gcc/bridge -c 8 -n 4 --proc-type=auto -- -n 0
`

3. Implement your own Network Function Application
-------------

Currently, our platform only supports 16 clients.  This limit is defined in [./openNetVM/onvm/shared/common.h](https://github.com/sdnfv/openNetVM/blob/master/onvm/shared/common.h#L26).

The tag to specify each NFs ID is the client id, which is a number 0, 1, 2, ..., n. It is assigned when starting a new network function from the command line using the `-n` flag.

The openNetVM manager is built with three directories where one contains source code for the manager, [./onvm/onvm_mgr](https://github.com/sdnfv/openNetVM/tree/master/onvm/onvm_mgr), the second contains the NF Guest libraries that provide each NF a method of communication between the manager and other NFs, [./onvm/onvm_nf](https://github.com/sdnfv/openNetVM/tree/master/onvm/onvm_nf), and the third is a shared library that contains variables and definitions that both the manager and guest libraries need.

###3.1 NF Guest Library

The NF Guest Library provides functions to allow each NF to interface with the manager and other NFs.  This library provides the main communication protocol of the system.  To include it, add the line `#include "onvm_nflib.h"` to the top of your c file.

It provides two functions that NFs must include: `int onvm_nf_init(int argc, char *argv[], struct onvm_nf_info *info)` and `int onvm_nf_run(struct onvm_nf_info *info void(*handler)(struct rte_mbuf *pkt, struct onvm_pkt_meta* meta))`.

The first function, `int onvm_nf_init(int argc, char *argv[], struct onvm_nf_info* info)`, initializes all the data structures and memory regions that the NF needs run and communicates with the manager about its existence.

The second function, `int onvm_nf_run(struct onvm_nf_info* info, void(*handler)(struct rte_mbuf* pkt, struct onvm_pkt_meta* meta))`, is the communication protocol between NF and manager where the NF is providing a packet handler function pointer to the manager.  The manager uses this function pointer to pass packets to when they arrive for a specific NF.  It continuously loops and listens for new packets and passes them on to the packet handler function.

###3.2 TCP/UDP Helper Library

openNetVM also provides a TCP/UDP Helper Library to provide an abstraction to extract information from the packets, which allow NFs to perform more complicated processing of packets.

There is one function in [./onvm/shared/onvm_pkt_helper.h](https://github.com/sdnfv/openNetVM/blob/master/onvm/shared/onvm_pkt_helper.h#L38) that can swap the source and destination MAC addresses of a packet.  It will return 0 if it is successful, something else if it is not.

+ `int onvm_pkt_mac_addr_swap(struct rte_mbuf *pkt, unsigned dst_port)`

Three functions in [./onvm/shared/onvm_pkt_helper.h](https://github.com/sdnfv/openNetVM/blob/master/onvm/shared/onvm_pkt_helper.h#L44) return pointers to the TCP, UDP, or IP headers in the packet.  If it is not a TCP, UDP, or IP packet, a NULL pointer will be returned.

+ `struct tcp_hdr* onvm_pkt_tcp_hdr(struct rte_mbuf* pkt);`
+ `struct udp_hdr* onvm_pkt_udp_hdr(struct rte_mbuf* pkt);`
+ `struct ipv4_hdr* onvm_pkt_ipv4_hdr(struct rte_mbuf* pkt);`

There are three functions in [./onvm/shared/onvm_pkt_helper.h](https://github.com/sdnfv/openNetVM/blob/master/onvm/shared/onvm_pkt_helper.h#L56) that determine whether the packet is a TCP, UDP, or IP packet.  If it is of one of these types, the functions will return 1 and 0 otherwise.

+ `int onvm_pkt_is_tcp(struct rte_mbuf* pkt);`
+ `int onvm_pkt_is_udp(struct rte_mbuf* pkt);`
+ `int onvm_pkt_is_ipv4(struct rte_mbuf* pkt);`

There are four functions in [./onvm/shared/onvm_pkt_helper.h](https://github.com/sdnfv/openNetVM/blob/master/onvm/shared/onvm_pkt_helper.h#L68) that print the whole packet or individual headers of the packet.  The first function below, `void onvm_pkt_print(struct rte_mbuf *pkt)`, prints out the entire packet.  It does this by calling the other three helper functions, which can be used on their own too.

+ `void onvm_pkt_print(struct rte_mbuf* pkt); `
+ `void onvm_pkt_print_tcp(struct tcp_hdr* hdr);`
+ `void onvm_pkt_print_udp(struct udp_hdr* hdr);`
+ `void onvm_pkt_print_ipv4(struct ipv4_hdr* hdr);`

Controlling packets in openNetVM is done with actions.  There are four actions, Drop, Next, To NF, and Out that control the flow of a packet.  These are defined in [./openNetVM/onvm/shared/common.h](https://github.com/sdnfv/openNetVM/blob/master/onvm/shared/common.h#L28).  The action is combined with a destination NF ID or a NIC Port ID, which determines where the packet ends up.
+ Drop packet:
	`#define ONVM_NF_ACTION_DROP 0 `
+ Forward using the rule from the SDN controller stored in the flow table:
	`#define ONVM_NF_ACTION_NEXT 1 `
+ Forward packets by specifying network function IDs:
	`#define ONVM_NF_ACTION_TONF 2`
+ Forward packets by specifying destination NIC port number:
	`#define ONVM_NF_ACTION_OUT 3 `

###3.3 NF Template and Makefile

####File: CHANGE_ME.c

```c
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_ip.h>

#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"
#include "common.h"
#include "init.h"
#include "args.h"

//------TODO: Include your header files here ------


//------TODO:  Modify print_delay variable, to tell the system incoming packet interval to print stats  ------
/* number of package between each print */
static uint32_t print_delay = 1000000;


//------TODO: Packet information function below prints information about incoming packets.  Build off of if for packet output ------
static void
packet_information(void){
        const char clr[] = { 27, '[', '2', 'J', '\0' };
        const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };
        static int pkt_process = 0;
        struct ipv4_hdr* ip;

        pkt_process += print_delay;

        /* Clear screen and move to top left */
        printf("%s%s", clr, topLeft);

        printf("PACKETS\n");
        printf("-----\n");
        printf("Port : %d\n", pkt->port);
        printf("Size : %d\n", pkt->pkt_len);
        printf("N°   : %d\n", pkt_process);
        printf("\n\n");

        ip = onvm_pkt_ipv4_hdr(pkt);
        if (ip != NULL) {
                onvm_pkt_print(pkt);
        } else {
                printf("No IP4 header found\n");
        }
}


//------TODO: Modify packet handler function to process packets.  This is the important one - packet handler function dictates how each packet is processed. ------
static void
packet_handler(struct rte_mbuf* pkt, struct onvm_pkt_meta* action) {
        static uint32_t counter = 0;
        if (counter++ == print_delay) {
                packet_information(pkt);
                counter = 0;
        }

	   /*
    	* ONVM_NF_ACTION_OUT is for forwarding packets with destination in form of another port;
    	* ONVM_NF_ACTION_DROP is to drop the packets;
    	* ONVM_NF_ACTION_NEXT forward packets to the next item in the chain.
    	*                     E.g. an SDN controller can provide flow rules to populate a flow
        *                     table and the rules there can dictate what's next.
    	* ONVM_NF_ACTION_TONF is for forwarding packets with destination in form of another NFs id.
    	*/

        // TODO: Build packet processing rules using the macros explained above.
}


int main(int argc, char *argv[]) {
        struct onvm_nf_info info;
        int retval;

        if ((retval = onvm_nf_init(argc, argv, &info)) < 0)
                return -1;
        argc -= retval;
        argv += retval;

        if (parse_app_args(argc, argv) < 0)
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");

        onvm_nf_run(&info, &packet_handler);
        printf("If we reach here, program is ending");
        return 0;
}

```

####Makefile Templete

```make
ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif


# Default target, can be overriden by command line or environment
include $(RTE_SDK)/mk/rte.vars.mk
RTE_TARGET ?= x86_64-native-linuxapp-gcc


#------TODO: modify here to name your APP------
# binary name
APP = XXX

#------TODO: modify here to name your source code file------
# all source are stored in SRCS-y
SRCS-y := XXX.c

#------TODO: if you put custmized APP in example folder, then you don't have to modify------
#if you put custmized APP in example folder, then you don't have to change anything below
#library directories to link goes here
ONVM= $(SRCDIR)/../../onvm

#optimize your code with the target version of compiler
CFLAGS += $(WERROR_FLAGS) -O3
CFLAGS += -I$(ONVM)/onvm_nf
CFLAGS += -I$(ONVM)/shared

#link openNetVM libraries
LDFLAGS += $(ONVM)/onvm_nf/onvm_nf/x86_64-native-linuxapp-gcc/onvm_nflib.o
LDFLAGS += $(ONVM)/shared/shared/x86_64-native-linuxapp-gcc/onvm_pkt_helper.o
include $(RTE_SDK)/mk/rte.extapp.mk
```
