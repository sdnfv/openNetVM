NF Development
==

Overview
--
Currently, our platform supports 16 clients.  This limit is defined in [./openNetVM/onvm/shared/common.h].

The tag to specify an NFs Service ID is `-r`, which is a number 0, 1, 2, ..., n.  This is what is used to route network traffic between NFs.  The manager automatically assigns an instance ID upon the NF's creation.

The openNetVM manager is built with three directories where one contains source code for the manager, [./onvm/onvm_mgr], the second contains the NF Guest libraries that provide each NF a method of communication between the manager and other NFs, [./onvm/onvm_nf], and the third is a shared library that contains variables and definitions that both the manager and guest libraries need.

NF Guest Library
--

The NF Guest Library provides functions to allow each NF to interface with the manager and other NFs.  This library provides the main communication protocol of the system.  To include it, add the line `#include "onvm_nflib.h"` to the top of your c file.

It provides two functions that NFs must include: `int onvm_nf_init(int argc, char *argv[], struct onvm_nf_info *info)` and `int onvm_nf_run(struct onvm_nf_info *info void(*handler)(struct rte_mbuf *pkt, struct onvm_pkt_meta* meta))`.

The first function, `int onvm_nf_init(int argc, char *argv[], struct onvm_nf_info* info)`, initializes all the data structures and memory regions that the NF needs run and communicates with the manager about its existence.

The second function, `int onvm_nf_run(struct onvm_nf_info* info, void(*handler)(struct rte_mbuf* pkt, struct onvm_pkt_meta* meta))`, is the communication protocol between NF and manager where the NF is providing a packet handler function pointer to the manager.  The manager uses this function pointer to pass packets to when they arrive for a specific NF.  It continuously loops and listens for new packets and passes them on to the packet handler function.

TCP/IP UDP Library
--

openNetVM also provides a TCP/IP UDP Helper Library to provide an abstraction to extract information from the packets, which allow NFs to perform more complicated processing of packets.

There is one function in [./onvm/shared/onvm_pkt_helper.h] that can swap the source and destination MAC addresses of a packet.  It will return 0 if it is successful, something else if it is not.

+ `int onvm_pkt_mac_addr_swap(struct rte_mbuf *pkt, unsigned dst_port)`

Three functions in [./onvm/shared/onvm_pkt_helper.h] return pointers to the TCP, UDP, or IP headers in the packet.  If it is not a TCP, UDP, or IP packet, a NULL pointer will be returned.

+ `struct tcp_hdr* onvm_pkt_tcp_hdr(struct rte_mbuf* pkt);`
+ `struct udp_hdr* onvm_pkt_udp_hdr(struct rte_mbuf* pkt);`
+ `struct ipv4_hdr* onvm_pkt_ipv4_hdr(struct rte_mbuf* pkt);`

There are three functions in that determine whether the packet is a TCP, UDP, or IP packet.  If it is of one of these types, the functions will return 1 and 0 otherwise.

+ `int onvm_pkt_is_tcp(struct rte_mbuf* pkt);`
+ `int onvm_pkt_is_udp(struct rte_mbuf* pkt);`
+ `int onvm_pkt_is_ipv4(struct rte_mbuf* pkt);`

There are four functions in [./onvm/shared/onvm_pkt_helper.h] that print the whole packet or individual headers of the packet.  The first function below, `void onvm_pkt_print(struct rte_mbuf *pkt)`, prints out the entire packet.  It does this by calling the other three helper functions, which can be used on their own too.

+ `void onvm_pkt_print(struct rte_mbuf* pkt); `
+ `void onvm_pkt_print_tcp(struct tcp_hdr* hdr);`
+ `void onvm_pkt_print_udp(struct udp_hdr* hdr);`
+ `void onvm_pkt_print_ipv4(struct ipv4_hdr* hdr);`

Controlling packets in openNetVM is done with actions.  There are four actions, Drop, Next, To NF, and Out that control the flow of a packet.  These are defined in [./openNetVM/onvm/shared/common.h].  The action is combined with a destination NF ID or a NIC Port ID, which determines where the packet ends up.
+ Drop packet:
	`#define ONVM_NF_ACTION_DROP 0 `
+ Forward using the rule from the SDN controller stored in the flow table:
	`#define ONVM_NF_ACTION_NEXT 1 `
+ Forward packets to the specified network function service ID:
	`#define ONVM_NF_ACTION_TONF 2`
+ Forward packets by specifying destination NIC port number:
	`#define ONVM_NF_ACTION_OUT 3 `


[./openNetVM/onvm/shared/common.h]: https://github.com/sdnfv/openNetVM/blob/master/onvm/shared/common.h#L26
[./onvm/onvm_mgr]: https://github.com/sdnfv/openNetVM/tree/master/onvm/onvm_mgr
[./onvm/onvm_nf]: https://github.com/sdnfv/openNetVM/tree/master/onvm/onvm_nf
[./onvm/shared/onvm_pkt_helper.h]: https://github.com/sdnfv/openNetVM/blob/master/onvm/shared/onvm_pkt_helper.h#L38
[./onvm/shared/onvm_pkt_helper.h]: https://github.com/sdnfv/openNetVM/blob/master/onvm/shared/onvm_pkt_helper.h#L44
[./onvm/shared/onvm_pkt_helper.h]: https://github.com/sdnfv/openNetVM/blob/master/onvm/shared/onvm_pkt_helper.h#L56
[./onvm/shared/onvm_pkt_helper.h]: https://github.com/sdnfv/openNetVM/blob/master/onvm/shared/onvm_pkt_helper.h#L68
[./openNetVM/onvm/shared/common.h]: https://github.com/sdnfv/openNetVM/blob/master/onvm/shared/common.h#L28