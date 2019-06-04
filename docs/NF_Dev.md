NF Development
==

Overview
--

The openNetVM manager is comprised of two directories: one containing the source code for the [manager][onvm_mgr] and the second containing the source for the [NF_Lib][onvm_nflib].  The manager is responsible for assigning cores to NFs, maintaining state bewteen NFs, routing packets between NICs and NFs, and displaying log messages and/or statistics.  The NF_Lib contains useful libraries to initialize and run NFs and libraries to support NF capabilities: [packet helper][pkt_helper], [flow table][flow_table], [flow director][flow_director], [service chains][srvc_chains], and [message passing][msg_passing].

Currently, our platform supports at most 128 NF instances running at once with a maximum ID value of 32 for each NF. We currently support a maximum of 32 NF instances per service. These limits are defined in [onvm_common.h][onvm_common.h:L51].  These are parameters developed for experimentation of the platform, and are subject to change.

NFs are run with different arguments in three different tiers--DPDK configuration flags, openNetVM configuration flags, and NF configuration flags--which are separated with `--`.
  - DPDK configuration flags:
    + Flags to configure how DPDK is initialized and run.  NFs typically use these arguments:
      - `-l CPU_CORE_LIST -n 3 --proc-type=secondary`
  - openNetVM configuration flags:
    + Flags to configure how the NF is managed by openNetVM.  NFs can configure their service ID and, for debugging, their instance ID (the manager automatically assigns instance IDs, but sometimes it is useful to manually assign them). NFs can also select to share cores with other NFs and enable manual core selection that overrides the onvm_mgr core selection (if core is available), their time to live and their packet limit (which is a packet based ttl):

      - `-r SERVICE_ID [-n INSTANCE_ID] [-s SHARE_CORE] [-m MANUAL_CORE_SELECTION] [-t TIME_TO_LIVE] [-l PACKET_LIMIT]`
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
  - `int onvm_nflib_init(int argc, char *argv[], const char *nf_tag, struct onvm_nf_info** nf_info_p)`, initializes all the data structures and memory regions that the NF needs run and communicates with the manager about its existence. Fills the passed double pointer with the onvm_nf_info struct. This is required to be called in the main function of an NF.
  - `int onvm_nflib_run(struct onvm_nf_info* info, void(*handler)(struct rte_mbuf* pkt, struct onvm_pkt_meta* meta))`, is the communication protocol between NF and manager, where the NF provides a pointer to a packet handler function to the manager.  The manager uses this function pointer to pass packets to the NF as it is routing traffic.  This function continuously loops, giving packets one-by-one to the destined NF as they arrive.

### Advanced Ring Manipulation
For advanced NFs, calling `onvm_nf_run` (as described above) is actually optional. There is a second mode where NFs can interface directly with the shared data structures.  Be warned that using this interface means the NF is responsible for its own packets, and the NF Guest Library can make fewer guarantees about overall system performance.  The advanced rings NFs are also responsible for managing their own cores, the NF can call the `onvm_threading_core_affinitize(nf_info->core)` function, the `nf_info->core` will have the  core assigned by the manager. Additionally, the NF is responsible for maintaining its own statistics.  An advanced NF can call `onvm_nflib_get_nf(uint16_t id)` to get the reference to `struct onvm_nf`, which has `struct rte_ring *` for RX and TX, a stat structure for that NF, and the `struct onvm_nf_info`. Alternatively NF can call `onvm_nflib_get_rx_ring(struct onvm_nf_info *info)` or `onvm_nflib_get_tx_ring(struct onvm_nf_info *info)` to get the `struct rte_ring *` for RX and TX, respectively. Finally, note that using any of these functions precludes you from calling `onvm_nf_run`, and calling `onvm_nf_run` precludes you from calling any of these advanced functions (they will return `NULL`).  The first interface you use is the one you get. To start receiving packets, you must first signal to the manager that the NF is ready by calling `onvm_nflib_nf_ready`.  
Example use of Advanced Rings can be seen in the speed_tester NF or the scaling example NF.

### Multithreaded NFs, scaling
NFs can scale by running multiple threads. For launching more threads the main NF had to be launched with more than 1 core. For running a new thread the NF should call `onvm_nflib_scale(struct onvm_nf_scale_info *scale_info)`. The `struct scale_info` has all the required information for starting a new child NF, service and instance ids, NF state data, and the packet handling functions. The struct can be obtained either by calling the `onvm_nflib_get_empty_scaling_config(struct onvm_nf_info *parent_info)` and manually filling it in or by inheriting the parent behavior by using `onvm_nflib_inherit_parent_config(struct onvm_nf_info *parent_info)`. As the spawned NFs are threads they will share all the global variables with its parent, the `onvm_nf_info->data` is a void pointer that should be used for NF state data.
Example use of Multithreading NF scaling functionality can be seen in the scaling_example NF.

### Shared core mode
This is an **EXPERIMENTAL** mode for OpenNetVM. It allows multiple NFs to run on a shared core.  In "normal" OpenNetVM, each NF will poll its RX queue for packets, monopolizing the CPU even if it has a low load.  This branch adds a semaphore-based communication system so that NFs will block when there are no packets available.  The NF Manger will then signal the semaphore once one or more packets arrive.

This code allows you to evaluate resource management techniques for NFs that share cores, however it has not been fully tested with complex NFs, therefore if you encounter any bugs please create an issue or a pull request with a proposed fix.

The code is based on the hybrid-polling model proposed in [_Flurries: Countless Fine-Grained NFs for Flexible Per-Flow Customization_ by Wei Zhang, Jinho Hwang, Shriram Rajagopalan, K. K. Ramakrishnan, and Timothy Wood, published at _Co-NEXT 16_][flurries_paper] and extended in [_NFVnice: Dynamic Backpressure and Scheduling for NFV Service Chains_ by Sameer G. Kulkarni, Wei Zhang, Jinho Hwang, Shriram Rajagopalan, K. K. Ramakrishnan, Timothy Wood, Mayutan Arumaithurai and Xiaoming Fu, published at _SIGCOMM '17_][nfvnice_paper]. Note that this code does not contain the full Flurries or NFVnice systems, only the basic support for shared-Core NFs. However, we have recently released a full version of the NFVNice system as an experimental branch, which can be found [here][nfvnice_branch].

Usage / Known Limitations:
  - To enable pass a `-c` flag to the onvm_mgr, and use a `-s` flag when starting a NF to specify that they want to share cores
  - All code for sharing CPUs is within `if (ONVM_NF_SHARE_CORES)` blocks
  - When enabled, you can run multiple NFs on the same CPU core with much less interference than if they are polling for packets
  - This code does not provide any particular intelligence for how NFs are scheduled or when they wakeup/sleep
  - Note that the manager threads all still use polling

Packet Helper Library
--

The openNetVM Packet Helper Library provides an abstraction to support development of NFs that use complex packet processing logic.  Here is a selected list of capablities that it can provide:

  - Swap the source and destination MAC addresses of a packet, then return 0 on success. `onvm_pkt_mac_addr_swap` can be found [here][onvm_pkt_helper.h:L56]
  - Check the packet type, either TCP, UDP, or IP.  If the packet type is verified, these functions will return 1.  They can be found [here][onvm_pkt_helper.h:L74]
  - Extract TCP, UDP, IP, or Ethernet headers from packets.  These functions return pointers to the respective headers in the packets.  If provided an unsupported packet header, a NULL pointer will be returned.  These are found [here][onvm_pkt_helper.h:L59]
  - Print the whole packet or individual headers of the packet.  These functions can be found [here][onvm_pkt_helper.h:L86].


Config File Library
--

The openNetVM Config File Library provides an abstraction that allows
NFs to load values from a JSON config file. While NFLib automatically
loads all DPDK and ONVM arguments when `-F` is passed, a developer can
add config support directly within the NF to support passing additional
values.

- NOTE: unless otherwise specified, all DPDK and ONVM arguments are **required**
- `onvm_config_parse_file(const char* filename)`: Load a JSON config file, and return a pointer to the cJSON struct.
- This is utilized to launch NFs using values specified in a config
  file. 
  `onvm_config_parse_file` can be found [here][onvm_config_common.h:L51]
- Additional config options can be loaded from within the NF, using cJSON. For further reference on how to access the values from the cJSON object, see the [cJSON docs](https://github.com/DaveGamble/cJSON)

### Sample Config File
```
{
  "dpdk": {
    "corelist": [STRING: corelist],
    "memory_channels": [INT: number of memory channels],
    "portmask": [INT: portmask]
  },

  "onvm": {
    "output": [STRING: output loc, either stdout or web],
    "serviceid": [INT: service ID for NF],
    "instanceid": [OPTIONAL, INT: this optional arg sets the instance ID of the NF]
  }
}
```

[onvm_mgr]: ../onvm/onvm_mgr
[onvm_nflib]: ../onvm/onvm_nflib
[onvm_nflib.h]: ../onvm/onvm_nflib/onvm_nflib.h
[onvm_pkt_helper.h:L56]: ../onvm/onvm_nflib/onvm_pkt_helper.h#L56
[onvm_pkt_helper.h:L59]: ../onvm/onvm_nflib/onvm_pkt_helper.h#L59
[onvm_pkt_helper.h:L74]: ../onvm/onvm_nflib/onvm_pkt_helper.h#L74
[onvm_pkt_helper.h:L86]: ../onvm/onvm_nflib/onvm_pkt_helper.h#L86
[onvm_config_common.h:L51]: ../onvm/onvm_nflib/onvm_config_common.h#L51
[onvm_config_common.h:L108]: ../onvm/onvm_nflib/onvm_config_common.h#L108
[onvm_common.h:L51]: ../onvm/onvm_nflib/onvm_common.h#L51
[onvm_common.h:L55]: ../onvm/onvm_nflib/onvm_common.h#L55
[forward]: ../examples/simple_forward/forward.c#L82
[pkt_helper]: ../onvm/onvm_nflib/onvm_pkt_helper.h
[flow_table]: ../onvm/onvm_nflib/onvm_flow_table.h
[flow_director]: ../onvm/onvm_nflib/onvm_flow_dir.h
[srvc_chains]: ../onvm/onvm_nflib/onvm_sc_common.h
[msg_passing]: ../onvm/onvm_nflib/onvm_msg_common.h
[flurries_paper]: https://dl.acm.org/citation.cfm?id=2999602
[nfvnice_paper]: https://dl.acm.org/citation.cfm?id=3098828
[nfvnice_branch]: https://github.com/sdnfv/openNetVM/tree/experimental/nfvnice-reinforce
