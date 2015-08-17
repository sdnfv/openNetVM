
:computer: openNetVM
===================

A high performance container-based NFV platform.
This guide is assuming that you are currently under ***./onvm*** directory and have already got openNetVM installed. 
 
As openNetVM is a ***container based*** Network Function platform, thus modifying and managing network functions for openNetVM is simple since the packets handling runs as user space processes inside Docker containers. 

Also, as Network Function Manager automatically conducts load balance packets across threads, ***scalability*** is achieved for maximizing performance.

Network functions such as monitors, hubs, bridges, switches, WAN optimizers, gardenwalls, firewalls etc. could be easily deployed on openNetVM platform, and network controller could provide rules under which network functions need to process each packet flow. In this repository, we have implemented two sample network functions, the monitor and the bridge to walk you through with omnipotent scalable openNetVM platform.

----------



:pencil2: 1. Execute openNetVM Manager 
-------------

openNetVM Manager is a controller which shows all running network functions in containers and distributes packets. It also has a monitor shows where each and every packets arrives and whether it is dropped or passed on to which network function clients. 


##1.1 Concise Instruction:


```
$sudo ./onvm_mgr/onvm_mgr/x86_64-native-linuxapp-gcc/onvm_mgr -c COREMASK<HEX NUM> -n NUM [ -- [-p PORTMASK<HEX NUM>] [-n NUM_CLIENTS<NUM>] ]


Options: 
	-c	an hexadecimal bit mask of the cores to run on. 
		Note that core numbering can change between platforms and should be determined beforehand.
	
	-n	number of memory channels per processor socket.
		Note that here we usually set it up with 4	


	--	secondary commands

		-p	an hexadecimal bit mask of the ports to run on.
			Note that here we usually use Oxff 

		-n	number of clients in total, here clients means network functions clients 
			Note that the lexical indexing starts from 0, which means, if you set n as 2, clients indexes range from [0, 1]
	
```

##1.2 Verbose Instruction

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



:pencil2: 2. Execute Network Function Clients
-------------

Network operators like you might have a set of functions that should be performed on different types of packtes flows. Here by modifying existing NFs, you could easily decide which types of network functions a packet flow should be processed by, and in what order.


NFV-based services could easily be implemented by minor modifying or adding patches to ***onvm_nf***, which make  performing flow management simple, and following are existing functions. 

##2.1 Concise Instruction


```
$sudo ./examples/basic_monitor/basic_monitor/x86_64-native-linuxapp-gcc/monitor -c COREMASK -n NUM  [-proc-type=<auto|primary|secondary>] [-- [-n CLIENT_ID] [-- [-n NF_ID] [-p <print_delay>] ] ]


Options: 

    -c	an hexadecimal bit mask of the cores to run on. 
        Note that core numbering can change between platforms and should be determined beforehand.

    -n	number of memory channels per processor socket.
        Note that here we usually set it up with 4.

    –proc-type	set the type of the current process.	
    		Note that -proc-type=<auto|primary|secondary> is set as auto in this case.

    -- secondary commands

        -n  client id number
            Note that client ID number ranges through [0, 1, 2, ...]

            -- third level commands

                -n	network function id number 
                
                -p	number of packets between each print.
                	Note that it is formated as '-p <print_delay>', e.g. -p 1 prints every packets

```



##2.2 Verbose Instruction


```
$sudo ./examples/basic_monitor/basic_monitor/x86_64-native-linuxapp-gcc/monitor -c COREMASK -n NUM -proc-type=<auto|primary|secondary> [-l CORELIST] [–lcores COREMAP] [-m MB] [-r NUM] [-v] [–syslog] [–socket-mem] [–huge-dir] [-- [-n CLIENT_ID] [-- -n NF_ID] [-p <print_delay>] ]


Options: 

    -c	an hexadecimal bit mask of the cores to run on. 
        Note that core numbering can change between platforms and should be determined beforehand.

    -n	number of memory channels per processor socket.
        Note that here we usually set it up with 4.

    –proc-type	set the type of the current process.	
        Note that -proc-type=<auto|primary|secondary> is set as auto in this case.

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


    -- secondary commands

        -n  client id number
            Note that client ID number ranges through [0, 1, 2, ...]

        -- third level commands

            -n network function id number 
                            
            -p	number of packets between each print.
                Note that it is formated as '-p <print_delay>', e.g. -p 1 prints every packets
            
            

```


On another server run `pktgen`, `MoonGen`, `httperf` or `ping` to verify that it works, please refer to markdown files located in docs directory if you are confused with running `MoonGen` and `pktgen`.



:pencil2: 3. Example 
------------- 


:pushpin:3.1 Application Monitor 

Monitor is an application build on openNetVM, which could show you the receive rate and drop rate from several clients. 
	
Please compile and build openNetVM first, and then execute openNetVM manager in one terminal window and run monitor application on another terminal window, at the same time use `pktgen` or `MoonGen` to send packets to your monitor node. If you have trouble with setting up with `pktgen` or `MoonGen`, please refer to markdown diles located in docs directory. 
	

To run openNetVM:

`
$sudo ./onvm/onvm_mgr/onvm_mgr/x86_64-native-linuxapp-gcc/onvm_mgr -c 6 -n 4 -- -p 1 -n 1
`

To run Monitor:

`
$sudo ./examples/basic_monitor/basic_monitor/x86_64-native-linuxapp-gcc/monitor -c 8 -n 4 --proc-type=auto -- -n 0 -- -p 100
`






:pushpin:3.2 Application Bridge

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

:blue_book: 4. Implement your own Network Function Application
------------- 


The maxium number for network function cleints created and running concurrently on this released version of openNetVM platform is ***16***, which is defined in `./openNetVM/onvm/shared/common.h` . 


###4.1 onvm_nf library

This library supports various network function utilities, including usage printing, network function client initialization and packets handling, if you would like to recall this library, the header file is located at `./openNetVM/onvm/onvm_nf/onvm_nflib.h`.

:pushpin: usage function: ***static void usage()***

`static void usage(const char *progname)` library provides an API for print a usage message, which will return guidance for options inputs when the formats are not recognizable by openNetVM. 




:pushpin: options argument parsing function: ***static int parse_nflib_args()***

`static int parse_nflib_args(int argc, char *argv[])` library supports the cunction of parsing the option inputs commands arguments.



:pushpin: initialize network functions function: ***int onvm_nf_init()***

`int onvm_nf_init(int argc, char *argv[], struct onvm_nf_info* info)` supports initialize network functions. 

:pushpin: packets handling function: ***int onvm_nf_run()***

`int onvm_nf_run(struct onvm_nf_info* info, void(*handler)(struct rte_mbuf* pkt, struct onvm_pkt_action* action))` is a library which continuously receives and processes packets. 



###4.2 onvm_mgr libraries



:pushpin: parsing arguments function: ***int parse_app_args()***

The `parse_app_args()` function is located at `./openNetVM/onvm/onvm_mgr/args.h`. 

The application specific arguments follow the DPDK-specific arguments which are stripped by the DPDK init. `int parse_app_args(uint8_t max_ports, int argc, char *argv[])`  processes these application arguments, printing usage information on error.

There are two specific options in this library, `-p`  envokes function `parse_portmask(max_ports, optarg)` , `-n` envokes `parse_num_clients(optarg)` function, and then, if the command typed by programmer does not match neither two, it prints out the usage error with anticipation. 


:pushpin: initialization function: ***int init()***

If you would like to utilize this function, it could be recalled by including `./openNetVM/onvm/onvm_mgr/init.h`.

`int init(int argc, char *argv[])` function is the fucntion which supports multi-process server applications, which calls subfunctions to do each stage of the initialization. 

In this function, bounch of initialization functions are recalled  sequentially to envoke a new process, firstly `rte_eal_init(argc, argv)` is recalled to initialize EAL, `rte_eth_dev_count()` is recalled to total number of ports, then `rte_memzone_reserve(MZ_CLIENT_INFO, sizeof(*clients_stats), rte_socket_id(), NO_FLAGS)` is recalled to set up array for client tx data. 

Ports information are set up by recalling `init_port(ports->id[i])`, which initializes a port, by configuring number of rx and tx rings, setting up each rx ring, to pull from the main mbuf pool, setting up each tx ring and then starting the port and report its status to stdout. `init_mbuf_pools()` is utilized and envoked to initialize mbuf pools. Last but not least, a function `init_shm_rings()` is recalled to initialize the client queues/rings for internal communications, each client has a RX queue, which passes packets, via pointers, between the multi-process server and client processes.

:pushpin: parsing function: `int parse_app_args()`


     



###4.3 shared libraries

Shared libraries are libraries shared by openNetVM manager and openNetVM network functions. 

```
#define ONVM_NF_ACTION_DROP 0  // drop packet
#define ONVM_NF_ACTION_NEXT 1  // to whatever the next action is configured by the SDN controller in the flow table
#define ONVM_NF_ACTION_TONF 2  // send to the NF specified in the argument field (assume it is on the same host)
#define ONVM_NF_ACTION_OUT 3   // send the packet out the NIC port set in the argument field
```










          
