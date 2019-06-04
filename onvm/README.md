openNetVM
==
openNetVM is comprised of a manager, NF library, and a TCP/IP library.

Manager
--
The openNetVM manager is responsible for orchestrating traffic between NFs.  It handles all Rx/Tx traffic in and out of the system, dynamically manages NFs starting and stopping, and it displays statistics regarding all traffic.

```
$sudo ./onvm_mgr/onvm_mgr/x86_64-native-linuxapp-gcc/onvm_mgr -l CORELIST -n MEMORY_CHANNELS --proc-type=primary -- -p PORTMASK -n NF_COREMASK [-r NUM_SERVICES] [-d DEFAULT_SERVICE] [-s STATS_OUTPUT] [-t TIME_TO_LIVE] [-l PACKET_LIMIT] 

Options:

        -c      an hexadecimal bit mask of the cores to run on.

        -n      number of memory channels per processor socket.

        -l      comma deliniated list of cores to run on.

        –lcores map lcore set to physical cpu set.

        -m      memory to allocate.

        -r      set the number of memory ranks (auto-detected by default).

        -v      display the version information on startup.

        -syslog set the syslog facility.

        --socket_mem    set the memory to allocate on specific sockets.

        --huge-dir      directory where the hugetlbfs is mounted.

        -–proc-type     set the type of the current process.

        --      secondary commands

                -p      a hexadecimal bit mask of the ports to use.

                -n      a hexadecimal bit mask of cores for NFs to run on.

                -r      an integer specifying the number of services.

                -d      an integer specifying the default service id.

                -s      a string (stdout/stderr/web) specifying where to
                        output statistics.

                -v      an integer specifying verbosity level

                -c      flag to enable shared_cpu mode

                -t      an integer specifying the time to live in seconds

                -l      an integer specifying the RX packet limit in 
                        Millions of pkts 
```

Usage
--
If the NFs crash with an error similar to `Cannot mmap memory for rte_config at [0x7ffff7ff3000], got [0x7ffff7ff2000] - please use '--base-virtaddr' option`, please use the `-a 0x7f000000000` flag for the onvm_mgr, this will resolve the issue.

NF Library
--
The NF Library is responsible for providing an interface for NFs to communicate with the manager.  It provides functions to initialize and send/receive packets to and from the manager.  This library provides the manager with a function pointer to the NF's `packet_handler`.

Packet Helper Library
--
The Packet Helper Libary provides an interface to extract TCP/IP, UDP, and other packet headers that were lost due to [Intel DPDK][dpdk].  Since DPDK avoides the Linux Kernel to bring packet data into userspace, we lose the encapsulation and decapsulation that the Kernel provides.  DPDK wraps packet data inside its own structure, the `rte_mbuf`.

Stats
--

There are 2 ways to view onvm_mgr stats:  

1. Console stats provide a detailed overview of all statistics and provide different verbosity modes.

    The default mode displays only the most critical NF information:
    ```
    PORTS
    -----
    Port 0: '90:e2:ba:b3:bc:6c'

    Port 0 - rx:         4  (        0 pps) tx:         0  (        0 pps)

    NF TAG         IID / SID / CORE    rx_pps  /  tx_pps        rx_drop  /  tx_drop           out   /    tonf     /   drop
    ----------------------------------------------------------------------------------------------------------------------
    speed_tester    1  /  1  /  4      1693920 / 1693920               0 / 0                      0 / 40346970    / 0
    ```

    The verbose mode (`-v`) adds `PNT`(Parent ID), `S|W`(NF state, sleeping or working), `CHLD`(Children count), as well as additional packet processing information:
    ```
    PORTS
    -----
    Port 0: '90:e2:ba:b3:bc:6c'

    Port 0 - rx:         4  (        0 pps) tx:         0  (        0 pps)

    NF TAG         IID / SID / CORE    rx_pps  /  tx_pps        rx_drop  /  tx_drop           out   /    tonf     /   drop
                   PNT / S|W / CHLD  drop_pps  /  drop_pps      rx_drop  /  tx_drop           next  /    buf      /   ret
    ----------------------------------------------------------------------------------------------------------------------
    speed_tester    1  /  1  /  4      9661664 / 9661664        94494528 / 94494528               0 / 94494487    / 0
                    0  /  W  /  0            0 / 0                     0 / 0                      0 / 0           / 128
    ```

    The shared core mode (`-c`) adds wakeup information stats:
    ```
    PORTS
    -----
    Port 0: '90:e2:ba:b3:bc:6c'

    Port 0 - rx:         5  (        0 pps) tx:         0  (        0 pps)

    NF TAG         IID / SID / CORE    rx_pps  /  tx_pps        rx_drop  /  tx_drop           out   /    tonf     /   drop
                   PNT / S|W / CHLD  drop_pps  /  drop_pps      rx_drop  /  tx_drop           next  /    buf      /   ret
                                      wakeups  /  wakeup_rt
    ----------------------------------------------------------------------------------------------------------------------
    simple_forward  2  /  2  /  4        27719 / 27719            764439 / 764439                 0 / 764439      / 0
                    0  /  S  /  0            0 / 0                     0 / 0                      0 / 0           / 0
                                        730557 / 25344

    speed_tester    3  /  1  /  5        27719 / 27719            764440 / 764439                 0 / 764440      / 0
                    0  /  W  /  0            0 / 0                     0 / 0                      0 / 0           / 1
                                        730560 / 25347



    Shared core stats
    -----------------
    Total wakeups = 1461122, Wakeup rate = 50696
    ```

    The super verbose stats mode (`-vv`) dumps all stats in a comma separated list for easy script parsing:
    ```
    #YYYY-MM-DD HH:MM:SS,nic_rx_pkts,nic_rx_pps,nic_tx_pkts,nic_tx_pps
    #YYYY-MM-DD HH:MM:SS,nf_tag,instance_id,service_id,core,parent,state,children_cnt,rx,tx,rx_pps,tx_pps,rx_drop,tx_drop,rx_drop_rate,tx_drop_rate,act_out,act_tonf,act_drop,act_next,act_buffer,act_returned,num_wakeups,wakeup_rate
    2019-06-04 08:54:52,0,4,4,0,0
    2019-06-04 08:54:53,0,4,0,0,0
    2019-06-04 08:54:54,simple_forward,1,2,4,0,W,0,29058,29058,29058,29058,0,0,0,0,0,29058,0,0,0,0,28951,28951
    2019-06-04 08:54:54,speed_tester,2,1,5,0,S,0,29058,29058,29058,29058,0,0,0,0,0,29059,0,0,0,1,28952,28952
    2019-06-04 08:54:55,0,4,0,0,0
    2019-06-04 08:54:55,simple_forward,1,2,4,0,W,0,101844,101843,72785,72785,0,0,0,0,0,101843,0,0,0,0,101660,101660
    2019-06-04 08:54:55,speed_tester,2,1,5,0,W,0,101844,101843,72785,72785,0,0,0,0,0,101844,0,0,0,1,101660,101660
    ```


2. Web Stats provide an easy to navigate web view with NF performance graphs, manager port stats and the core layout across the system. It also keeps track of timestamps for NF events such as NF_STARTING and NF_STOPPING. 

    For more info and design details check the [web stats docs][web_stats_docs]

[dpdk]: http://dpdk.org/
[web_stats_docs]: ../onvm_web/README.md
