Frequently Asked Questions
=====================================

This page answers common questions about OpenNetVM's Architecture.

Does OpenNetVM use Virtual Machines?
-------------------------------------
Not anymore!  Originally, the platform was designed to run KVM-based virtual machines. However, now we use regular processes for NFs, which can be optionally deployed inside containers.  The name has been kept for historical reasons.

What are the system's main components?
----------------------------------------
An OpenNetVM host runs the Manager plus one or more Network Functions.

The OpenNetVM manager has a minimum of three threads:
- The `RX Thread(s) <https://github.com/sdnfv/openNetVM/blob/46bbc962a0ef2ddfd774a7fda798f9ea92b7b116/onvm/onvm_mgr/main.c#L182>`_ receives packets from the NIC and delivers them to the first NF in a chain. By default there is only 1 RX thread, but this can be changed with the :code:`ONVM_NUM_RX_THREADS` macro in `onvm_init.h <https://github.com/sdnfv/openNetVM/blob/46bbc962a0ef2ddfd774a7fda798f9ea92b7b116/onvm/onvm_mgr/onvm_init.h#L96>`_
- The `TX Thread(s) <https://github.com/sdnfv/openNetVM/blob/46bbc962a0ef2ddfd774a7fda798f9ea92b7b116/onvm/onvm_mgr/main.c#L217>`_ retrieves packets after NFs finish processing them, and sends them out the NIC. If you have multiple NIC ports you may need to increase the number of TX threads in the manager command line arguments.
- The `Master Stats Thread <https://github.com/sdnfv/openNetVM/blob/46bbc962a0ef2ddfd774a7fda798f9ea92b7b116/onvm/onvm_mgr/main.c#L77>`_ detects when new NFs start and helps initialize them. It also periodically prints out statistics.
- (Optional) The `Wakeup Thread(s) <https://github.com/sdnfv/openNetVM/blob/46bbc962a0ef2ddfd774a7fda798f9ea92b7b116/onvm/onvm_mgr/main.c#L280>`_ is used if shared CPU mode is enabled with the :code:`-c` argument. The thread checks whether any sleeping NFs have received packets and wakes them with a semaphore. This allows NFs to share CPUs without constantly polling for packets.

Network functions are typically a single thread with two portions:
- nflib provides the library functions to interact with the Manager. When NFs start they typically call :code:`onvm_nflib_run` to start the main run loop.
- When packets arrive for the NF, the nflib run loop will execute the NF's packet handler function. This should contain the application logic for the NF.
- Alternatively, NFs can use the Advanced Ring mode to have direct access to their incoming packet queues, allowing for more complex execution models.
- The :code:`onvm_nflib_scale` API can be used for NFs that require multiple threads. The Manager views each scaled thread as a distinct NF with its own packet queues.
- See the NF API page for more details.

Where are packets first received?
------------------------------------
Packets initially arrive on a queue on a NIC port. The manager's RX and TX thread handle routing of packets between NFs and the NIC ports. The RX thread will remove a batch of packets from a NIC queue and examine them individually to determine their defined actions (drop, send to another NF, or send out of the system). The TX thread performs a similar function, but instead of reading from a NIC queue, they read from the transmit ring buffers of NFs and either send the packets out a NIC port or to a different NF service ring.

How do NFs process packets?
-----------------------------
Network functions interact with the manager using the NFLib API. The `NFLib run function <https://github.com/sdnfv/openNetVM/blob/master/onvm/onvm_nflib/onvm_nflib.c#L557>`_ then polls its receive ring for packets arriving from the manager or other NFs by calling the function `onvm_nflib_thread_main_loop <https://github.com/sdnfv/openNetVM/blob/master/onvm/onvm_nflib/onvm_nflib.c#L572>`_. For each packet received the NFLib executes a call back function provided by the NF developer to process the packet. An example of this process can be seen in the `bridge <https://github.com/sdnfv/openNetVM/blob/master/examples/bridge/bridge.c#L170>`_ NF example. When a NF runs its callback to handle an incoming packet, it is given a packet descriptor that contains the address of the packet body and metadata. The NF will then perform the desired functionality.

What actions can NFs perform?
------------------------------
There are many `examples <https://github.com/sdnfv/openNetVM/tree/master/examples>`_ that show the powerful packet handling, messaging, and overall possible applications of Network Functions handled by the ONVM manager. The main NFs to jump into first are `Speed Tester <https://github.com/sdnfv/openNetVM/blob/master/examples/speed_tester/speed_tester.c>`_ and `Basic Monitor <https://github.com/sdnfv/openNetVM/blob/master/examples/basic_monitor/monitor.c>`__, because of their readability and easy command-line interfaces. Initially, NFs can be run with a great deal of mandatory/optional arguments. An example of this is `parse_app_args <https://github.com/sdnfv/openNetVM/blob/master/examples/speed_tester/speed_tester.c#L143>`_, which, for NFs like Firewall, can be used to input `json configurations <https://github.com/sdnfv/openNetVM/blob/master/examples/firewall/firewall.c#L134>`_. By including :code:`#include "cJSON.h"`, an NF can parse a JSON file and instantiate a cJSON struct with `onvm_config_parse_file <https://github.com/sdnfv/openNetVM/blob/master/examples/firewall/firewall.c#L319>`_. Doing this can help with scalable testing for many types of NFs working together. For examples of how to pass NF/Manager-specific arguments, see the NF start scripts `start_nf.sh <https://github.com/sdnfv/openNetVM/blob/master/examples/start_nf.sh>`_ and `go.sh <https://github.com/sdnfv/openNetVM/blob/master/examples/go.sh>`_.

ONVM provides a lot of packet parsing APIs that are crucial for handling different types of data, through either UDP or TCP connections. These `definitions <https://github.com/sdnfv/openNetVM/blob/master/onvm/onvm_nflib/onvm_pkt_helper.h>`_ include checking the packet type, swapping mac address of the destination, parsing the IP address, and many more useful functions. NFs also have access to packet meta data defined `here <https://github.com/sdnfv/openNetVM/blob/master/onvm/onvm_nflib/onvm_common.h#L108>`_ which can be tracked and updated depending on the purpose of the NF. This allows NFs to forward, drop, send packets to another NF, defining the primary packet handler, such as a Firewall, and secondaries that receive the valid forwarded packets. More information on sending packets can be seen below in **How do packets move between network functions**. 

In main of each NF, a few functions should be called to set signals, data structures, and the NF context, handled by the running manager. These include `onvm_nflib_init <https://github.com/sdnfv/openNetVM/blob/master/onvm/onvm_nflib/onvm_nflib.c#L304>`_, `onvm_nflib_init_nf_local_ctx <https://github.com/sdnfv/openNetVM/blob/master/onvm/onvm_nflib/onvm_nflib.c#L236>`_, and `onvm_nflib_start_signal_handler <https://github.com/sdnfv/openNetVM/blob/master/onvm/onvm_nflib/onvm_nflib.c#L291>`_. There are certainly others, most of which are demonstrated in the main functions of the example NFs, such as `Speed Tester <https://github.com/sdnfv/openNetVM/blob/master/examples/speed_tester/speed_tester.c#L419>`__. With these initializations, NFs set communication lines between themselves and the manager, so packets are handled according to the NFs job. For example, while Firewall's :code:`nf_function_table->pkt_handler` is `packet_handler <https://github.com/sdnfv/openNetVM/blob/master/examples/firewall/firewall.c#L198>`_, it doesn't have a :code:`user_actions` function callback like `Basic Monitor <https://github.com/sdnfv/openNetVM/blob/master/examples/basic_monitor/monitor.c#L149>`__ does. This is ok, it just shows the flexibility that NFs have to carry out the tasks they want with the packets that have been passed to them from the queue.

Finally, a recent major development to the ONVM platform has been scalable NFs running through shared cores. Previously, each NF was designated their own core to run on, but enabling shared cores in the manager will now allow multiple NFs running on the same core. As shown in the stats output, if the manager :code:`go.sh` is run with :code:`-c`, NFs will be put to sleep when no packets are sent to them, and awoken through signals when they have something to do. A great example to get started is running `Scaling <https://github.com/sdnfv/openNetVM/blob/master/examples/scaling_example/scaling.c>`_, where a number of children can be passed by enabling with :code:`-c` and set with :code:`-n `. All of these options show the different capabilities of network functions in onvm. Read through the documentation on each NF mentioned here to get a better idea of the pieces that can be extracted to combine with another NF.

How do packets move between network functions?
-----------------------------------------------
When a network function is instantiated, a `struct onvm_nf <https://github.com/sdnfv/openNetVM/blob/master/onvm/onvm_nflib/onvm_common.h#L261>`_ structure is created alongside it. Along with important pieces of network function specific values, it also contains a ring buffer to store packets. The important field values involving NF to NF communication are the :code:`rx_ring` and the :code:`nf_tx_mgr` data structures.

If a network function wishes to send a packet to another network functions :code:`rx_ring`, it must modify the packets metadata within the sending network functions :code:`packet_handler`. The `onvm_pkt_meta <https://github.com/sdnfv/openNetVM/blob/master/onvm/onvm_nflib/onvm_common.h#L108>`_ allows for this metadata implementation, which allows the `main network function loop <https://github.com/sdnfv/openNetVM/blob/master/onvm/onvm_nflib/onvm_nflib.c#L557>`_ to understand what to do next with the packet (drop, send to another NF, or send out of the system). 

An example of this process can be seen in the `simple_forward <https://github.com/sdnfv/openNetVM/blob/master/examples/simple_forward/forward.c#L162>`_ network function.
Here, the packet_handler receives a packet and proceeds to modify its corresponding meta-data. :code:`meta->action` must be set to :code:`ONVM_NF_TONF` along with the destination (service ID) of the intended recipient. Upon completion, the main network function loop will enqueue the packets onto the recipient network functions :code:`rx_ring`.
 
