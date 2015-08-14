
openNetVM
===================

A high performance container-based NFV platform.
This guide is assuming that you are currently under simple-onvm directory and have already got openNetVM installed. 
 
As openNetVM is a container based Network Function platform, thus modifying and managing network functions for openNetVM is simple since the packets handling runs as user space processes inside Docker containers. 

Network functions such as monitors, hubs, bridges, switches, WAN optimizers, gardenwalls, firewalls etc. could be easily deployed on openNetVM platform, and network controller could provide rules under which network functions need to process each packet flow.

----------



#1. Execute openNetVM Manager 
-------------

openNetVM Manager is a controller 

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



#2. Execute Network Function Clients
-------------


##2.1 Concise Instruction


```
$sudo ./onvm_nf/onvm_nf/x86_64-native-linuxapp-gcc/onvm_nf -c COREMASK -n NUM  [-proc-type=<auto|primary|secondary>] [-- [-n CLIENT_ID] [-- [-n NF_ID] [-p <print_delay>] ] ]


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
$sudo ./onvm_nf/onvm_nf/x86_64-native-linuxapp-gcc/onvm_nf -c COREMASK -n NUM -proc-type=<auto|primary|secondary> [-l CORELIST] [–lcores COREMAP] [-m MB] [-r NUM] [-v] [–syslog] [–socket-mem] [–huge-dir] [-- [-n CLIENT_ID] [-- -n NF_ID] [-p <print_delay>] ]


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


On another server run `pktgen`, `MoonGen`, `httperf` or `ping` to verify that it works, please refer to markdown diles located in docs directory if you are confused with running `MoonGen` and `pktgen`.



#3. Example 

------------- 


##3.1 Application Monitor 

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






##3.2 Application Bridge

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










          
