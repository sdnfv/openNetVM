NF Development
==

Overview
--

The openNetVM manager is comprised of two directories: one containing the source code for the [manager][onvm_mgr] and the second containing the source for the [NF_Lib][onvm_nflib].  The manager is responsible for maintaining state bewteen NFs, routing packets between NICs and NFs, and displaying log messages and/or statistics.  The NF_Lib contains useful libraries to initialize and run NFs and libraries to support NF capabilities: [packet helper][pkt_helper], [flow table][flow_table], [flow director][flow_director], [service chains][srvc_chains], and [message passing][msg_passing].

Currently, our platform supports at most 16 NF instances running at once.  This limit is defined in [onvm_common.h][onvm_common.h:L51].

NFs are run with different arguments in three different tiers--DPDK configuration flags, openNetVM configuration flags, and NF configuration flags--which are separated with `--`.
  - DPDK configuration flags:
    + Flags to configure how DPDK is initialized and run.  NFs typically use these arguments:
      - `-l CPU_CORE_LIST -n 3 --proc-type=secondary`
  - openNetVM configuration flags:
    + Flags to configure how the NF is managed by openNetVM.  NFs can configure their service ID and, for debugging, their instance ID (the manager automatically assigns instance IDs, but sometimes it is useful to manually assign them):
      - `-r SERVICE_ID [-n INSTANCE_ID]`
  - NF configuration flags:
    + User defined flags to configure NF parameters.  Some of our example NFs use a flag to throttle how often packet info is printed, or to specify a destination NF to send packets to.  See the [simple_forward][forward] NF for an example of them both.

Each NF needs to have a packet handler function.  It must match this specification: `static int packet_handler(struct rte_mbuf* pkt, struct onvm_pkt_meta* meta);`  A pointer to this function will be provided to the manager and is the entry point for packets to be processed by the NF (see NF Library section below).  Once packet processing is finished, an NF can set an _action_ coupled with a _destination NF ID_ or _destination port ID_ in the packet which tell the openNetVM manager how route the packet next.  These _actions_ are defined in [onvm_common][onvm_common.h:L55]:
  - `ONVM_NF_ACTION_DROP`: Drop the packet
  - `ONVM_NF_ACTION_NEXT`: Forward the packet using the rule from the SDN controller stored in the flow table
  - `ONVM_NF_ACTION_TONF`: Forward the packet to the specified NF
  - `ONVM_NF_ACTION_OUT`: Forward the packet to the specified NIC port

NF Library
--

The NF_Lib Library provides functions to allow each NF to interface with the manager and other NFs.  This library provides the main communication protocol of the system.  To include it, add the line `#include "onvm_nflib.h"` to the top of your c file.

Here are some of the frequently used functions of this library (to see the full API, please review the [NF_Lib header][onvm_nflib.h]):
  - `int onvm_nf_init(int argc, char *argv[], struct onvm_nf_info* info)`, initializes all the data structures and memory regions that the NF needs run and communicates with the manager about its existence.  This is required to be called in the main function of an NF.
  - `int onvm_nf_run(struct onvm_nf_info* info, void(*handler)(struct rte_mbuf* pkt, struct onvm_pkt_meta* meta))`, is the communication protocol between NF and manager, where the NF provides a pointer to a packet handler function to the manager.  The manager uses this function pointer to pass packets to the NF as it is routing traffic.  This function continuously loops, giving packets one-by-one to the destined NF as they arrive.

### Advanced Ring Manipulation
For advanced NFs, calling `onvm_nf_run` (as described above) is actually optional. There is a second mode where NFs can interface directly with the shared data structures.  Be warned that using this interface means the NF is responsible for its own packets, and the NF Guest Library can make fewer guarantees about overall system performance.  Additionally, the NF is responsible for maintaining its own statistics.  An advanced NF can call `onvm_nflib_get_nf(uint16_t id)` to get the reference to `struct onvm_nf`, which has `struct rte_ring *` for RX and TX, a stat structure for that NF, and the `struct onvm_nf_info`. Alternatively NF can call `onvm_nflib_get_rx_ring(struct onvm_nf_info *info)` or `onvm_nflib_get_tx_ring(struct onvm_nf_info *info)` to get the `struct rte_ring *` for RX and TX, respectively. Finally, note that using any of these functions precludes you from calling `onvm_nf_run`, and calling `onvm_nf_run` precludes you from calling any of these advanced functions (they will return `NULL`).  The first interface you use is the one you get. To start receiving packets, you must first signal to the manager that the NF is ready by calling `onvm_nflib_nf_ready`.  
Example use of Advanced Rings can be seen in the speed_tester NF.

Packet Helper Library
--

The openNetVM Packet Helper Library provides an abstraction to support development of NFs that use complex packet processing logic.  Here is a selected list of capablities that it can provide:

  - Swap the source and destination MAC addresses of a packet, then return 0 on success. `onvm_pkt_mac_addr_swap` can be found [here][onvm_pkt_helper.h:L56]
  - Check the packet type, either TCP, UDP, or IP.  If the packet type is verified, these functions will return 1.  They can be found [here][onvm_pkt_helper.h:L74]
  - Extract TCP, UDP, IP, or Ethernet headers from packets.  These functions return pointers to the respective headers in the packets.  If provided an unsupported packet header, a NULL pointer will be returned.  These are found [here][onvm_pkt_helper.h:L59]
  - Print the whole packet or individual headers of the packet.  These functions can be found [here][onvm_pkt_helper.h:L86].


[onvm_mgr]: ../onvm/onvm_mgr
[onvm_nflib]: ../onvm/onvm_nflib
[onvm_nflib.h]: ../onvm/onvm_nflib/onvm_nflib.h
[onvm_pkt_helper.h:L56]: ../onvm/onvm_nflib/onvm_pkt_helper.h#L56
[onvm_pkt_helper.h:L59]: ../onvm/onvm_nflib/onvm_pkt_helper.h#L59
[onvm_pkt_helper.h:L74]: ../onvm/onvm_nflib/onvm_pkt_helper.h#L74
[onvm_pkt_helper.h:L86]: ../onvm/onvm_nflib/onvm_pkt_helper.h#L86
[onvm_common.h:L51]: ../onvm/onvm_nflib/onvm_common.h#L51
[onvm_common.h:L55]: ../onvm/onvm_nflib/onvm_common.h#L55
[forward]: ../examples/simple_forward/forward.c#L82
[pkt_helper]: ../onvm/onvm_nflib/onvm_pkt_helper.h
[flow_table]: ../onvm/onvm_nflib/onvm_flow_table.h
[flow_director]: ../onvm/onvm_nflib/onvm_flow_dir.h
[srvc_chains]: ../onvm/onvm_nflib/onvm_sc_common.h
[msg_passing]: ../onvm/onvm_nflib/onvm_msg_common.h

