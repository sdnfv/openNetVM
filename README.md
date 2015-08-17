
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


###4.1 onvm_nf libraries




:pushpin: usage library 

`static void usage(const char *progname)` library provides an API for print a usage message, which will return guidance for options inputs when the formats are not recognizable by openNetVM. 




:pushpin: options argument parsing library

`static int parse_nflib_args(int argc, char *argv[])` library supports the cunction of parsing the option inputs commands arguments.



:pushpin: initialize network functions library

`int onvm_nf_init(int argc, char *argv[], struct onvm_nf_info* info)` supports initialize network functions. 

:pushpin: packets handling library

`int onvm_nf_run(struct onvm_nf_info* info, void(*handler)(struct rte_mbuf* pkt, struct onvm_pkt_action* action))` is a library which continuously receives and processes packets. 



###4.2 onvm_mgr libraries

:pushpin: parsing arguments library

The application specific arguments follow the DPDK-specific arguments which are stripped by the DPDK init. `int parse_app_args(uint8_t max_ports, int argc, char *argv[])`  processes these application arguments, printing usage information on error.

There are two specific options in this library, `-p`  envokes function `parse_portmask(max_ports, optarg)` , `-n` envokes `parse_num_clients(optarg)` function, and then, if the command typed by programmer does not match neither two, it prints out the usage error with anticipation. 




###4.3 shared libraries

Shared libraries are libraries shared by openNetVM manager and openNetVM network functions. 










          
