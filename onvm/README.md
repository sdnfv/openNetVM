openNetVM
==
openNetVM is comprised of a manager, NF library, and a TCP/IP library.

Manager
--
The openNetVM manager is responsible for orchestrating traffic between NFs.  It handles all Rx/Tx traffic in and out of the system, dynamically manages NFs starting and stopping, and it displays statistics regarding all traffic.

```
$sudo ./onvm_mgr/onvm_mgr/x86_64-native-linuxapp-gcc/onvm_mgr -l CORELIST -n MEMORY_CHANNELS --proc-type=primary -- -p PORTMASK [-r NUM_SERVICES] [-d DEFAULT_SERVICE] [-s STATS_OUTPUT]

Options:

	-c	an hexadecimal bit mask of the cores to run on.

	-n	number of memory channels per processor socket.

	-l	comma deliniated list of cores to run on.

	–lcores	map lcore set to physical cpu set.

	-m	memory to allocate.

	-r	set the number of memory ranks (auto-detected by default).

	-v	display the version information on startup.

	-syslog	set the syslog facility.

	--socket_mem	set the memory to allocate on specific sockets.

	--huge-dir	directory where the hugetlbfs is mounted.

	-–proc-type	set the type of the current process.

	--	secondary commands

		-p	a hexadecimal bit mask of the ports to use.

		-r	an integer specifying the number of services.

		-d	an integer specifying the default service id.

		-s	a string (stdout/stderr/web) specifying where to
output statistics.
```

NF Library
--
The NF Library is responsible for providing an interface for NFs to communicate with the manager.  It provides functions to initialize and send/receive packets to and from the manager.  This library provides the manager with a function pointer to the NF's `packet_handler`.

Packet Helper Library
--
The Packet Helper Libary provides an interface to extract TCP/IP, UDP, and other packet headers that were lost due to [Intel DPDK][dpdk].  Since DPDK avoides the Linux Kernel to bring packet data into userspace, we lose the encapsulation and decapsulation that the Kernel provides.  DPDK wraps packet data inside its own structure, the `rte_mbuf`.

[dpdk]: http://dpdk.org/
