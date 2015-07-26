OpenNetVM Installation
===================




#### Welcome to installation memo for [OpenNetVM](https://www.usenix.org/conference/nsdi14/technical-sessions/presentation/hwang), OpenNetVM is build with [Docker Container](https://www.docker.com/) and Intel [DPDK](http://dpdk.org/) library, which offers high bandwidth network functions. 

----------


1. Check System
-------------

Before installation of OpenNetVM, please check your machine to see if it could meet the **minimum** requirements as following: 

1.1  check what NIC do you have by typing, see if your NIC belongs to one of [Supported NICs](http://dpdk.org/). 

 `$lspci | awk '/net/ {print $1}' | xargs -i% lspci -ks %`

1.2  check what operating system do you have by typing, your Kernel version should be higher than 2.6.33.

 `$uname -a`

1.3  check if your system supports uio

 `$locate uio`

1.4 if you could not locate uio in step 1.3, please install build-essential and  linux-headers, then try step 1.3 again

 `$sudo apt-get install build-essential linux-headers-$(uname -r) git`


2. Get Package 
-------------
2.1  install git

 `$sudo apt-get install git`

2.2  download	source	code

 `$git clone https://github.com/sdnfv/openNetVM`

2.3 switch to the branch of onvm

 `$cd  ./openNetVM/`
 `$git checkout simple-onvm`

2.4  enter working directory

 `$cd  ~/openNetVM/dpdk-1.8.0/`


3. Set	up Environment 
------------- 
3.1 check out current directory, and remember the output, e.g. /home/**your_name**/openNetVM/dpdk-1.8.0"

 `$pwd`

3.2  list all possible configurations in dpdk

 `$config/`

3.3  set environment variable RTE_SDK as the output you got from step 3.1,   e.g. "/home/**your_name**/openNetVM/dpdk-1.8.0"

 `$sudo sh -c "echo export RTE_SDK=/home/**your_name**/openNetVM/dpdk-1.8.0  >> ~/.bashrc"`

3.4  set  environment variable RTE_TARGET as one of the list of config files you got from step 3.2, e.g.  "x86_64-native-linuxapp-gcc"

 `$sudo sh -c "echo export RTE_TARGET=x86_64-native-linuxapp-gcc  >> ~/.bashrc"`

 `$source ~/.bashrc`

4.  Configure	and	compile	DPDK
------------- 
4.1 specify the configuration type the same as step 3.4,  e.g.  "x86_64-native-linuxapp-gcc" 

 `$make config T=x86_64-native-linuxapp-gcc`

4.2 install the exact same configuration you used in step 4.1, e.g.  "x86_64-native-linuxapp-gcc" 

 `$make install T=x86_64-native-linuxapp-gcc`

 `$make`


5. Create	Hugepage Directory and Reserve	Memory
------------- 
5.1  create a directory in your linux environment

 `$sudo mkdir -p /mnt/huge`

5.2 mount the directory you created in step 5.1  to memory formatted with huge table file system (hugetlbfs)

 `$sudo mount -t hugetlbfs nodev /mnt/huge`

5.3 create 1024 hugepages

 `$sudo sh -c "echo 1024 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages"`

6. Install Kernel Module
------------- 
6.1 after the installation, you will see a new folder appears in your directory, e.g  "x86_64-native-linuxapp-gcc" 

 `$ls` 

6.2 load in your uio linux kernel module

 `$sudo modprobe uio`

6.3 load in your igb_uio, which is in DPDK kernel module, e.g x86_64-native-linuxapp-gcc 

 `$sudo insmod x86_64-native-linuxapp-gcc/kmod/igb_uio.ko`

7. Bind	NIC to DPDK igb_uio Kernel Module
------------- 
7.1 check your current status of NIC binding and active status

 `$./x86_64-native-linuxapp-gcc/tools/dpdk_nic_bind.py  --status`

something as below will show up, in this case, the "82599EB 10-Gigabit" is the NIC we want to utilize for DPDK, notice that the one for log in the node should not be the "82599EB 10-Gigabit"
```
Network devices using DPDK-compatible driver
============================================
<none>

Network devices using kernel driver
===================================
0000:05:00.0 '82576 Gigabit Network Connection' if=eth0 drv=igb unused=igb_uio *Active*
0000:07:00.0 '82599EB 10-Gigabit SFI/SFP+ Network Connection' if=eth2 drv=ixgbe unused=igb_uio *Active*
0000:07:00.1 '82599EB 10-Gigabit SFI/SFP+ Network Connection' if=eth3 drv=ixgbe unused=igb_uio 
```

7.2 as you could see, the 10G one is active now, so the next thing is to turn it down

 `$sudo ifconfig eth2 down`
now we could check the status again

 `$./x86_64-native-linuxapp-gcc/tools/dpdk_nic_bind.py  --status`

7.3 bind the 10G to DPDK, notice that only port 0 is wired, so you would like to bind 07:00.0

 `$sudo ./x86_64-native-linuxapp-gcc/tools/ dpdk_nic_bind.py -b igb_uio 07:00.0`

7.4 check the status again, if it shows up as following, you are all set

 `$./x86_64-native-linuxapp-gcc/tools/dpdk_nic_bind.py  --status`

```
Network devices using DPDK-compatible driver
============================================
0000:07:00.0 '82599EB 10-Gigabit SFI/SFP+ Network Connection' drv=igb_uio unused=ixgbe

Network devices using kernel driver
===================================
0000:05:00.0 '82576 Gigabit Network Connection' if=eth0 drv=igb unused=igb_uio *Active*
0000:07:00.1 '82599EB 10-Gigabit SFI/SFP+ Network Connection' if=eth3 drv=ixgbe unused=igb_uio
```

8. Run	HelloWorld	Application
------------- 
8.1 enter working directory, and compile the application

 `$cd ./dpdk-1.8.0/examples/helloworld/`

 `$make`

8.2 executing the example

  `$sudo build/helloworld -c 3 -n 1`

if you got the last two line of your result as below, , then you are all set

```
hello from core 1
hello from core 0
```







