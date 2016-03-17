OpenNetVM Installation
===================

Welcome to the installation guide for **[OpenNetVM](https://http://sdnfv.github.io/onvm/)**, OpenNetVM uses [Docker Containers](https://www.docker.com/) to isolate NFs and the Intel [DPDK](http://dpdk.org/) library for high performance network I/O.

----------


1. Check System
-------------

Before installation of OpenNetVM, please check your machine to see if it could meet the **minimum** requirements as following:

1.1  check what NIC you have with command below and see if your NIC belongs to one of the [Supported NICs](http://dpdk.org/doc/nics/).

 `$lspci | awk '/net/ {print $1}' | xargs -i% lspci -ks %`

1.2  check what operating system you have by typing:

 `$uname -a`

 your Kernel version should be higher than 2.6.33.

1.3  check if your system supports uio

 `$locate uio`

1.4 if you could not locate uio in step 1.3, please install build-essential and  linux-headers, then try step 1.3 again

 `$sudo apt-get install build-essential linux-headers-$(uname -r) git`


2. Install Dependencies
-------------
2.1  install git

 `$sudo apt-get install git`

2.2  download source code

 `$git clone https://github.com/sdnfv/openNetVM`

2.3 Initialize dpdk submodule

  `$ git submodule init && git submodule update`

2.3  enter dpdk working directory

 `$cd  openNetVM/dpdk/`


3. Set	up Environment
-------------
3.1 check out current directory, and remember the output, e.g. "/home/**your_name**/openNetVM/dpdk"

 `$ pwd`

3.2  list all possible configurations in dpdk

 `$ ls ./config/`

3.3  set environment variable RTE_SDK as the output you got from step 3.1,   e.g. "/home/**your_name**/openNetVM/dpdk"

 `$ echo export RTE_SDK=/home/**your_name**/openNetVM/dpdk  >> ~/.bashrc`

 or if you are currently in the dpdk directory simply use:

 ``
 $ echo export RTE_SDK=`pwd` >> ~/.bashrc
 ``

3.4  set  environment variable RTE_TARGET as one of the list of config files you got from step 3.2, e.g.  "x86_64-native-linuxapp-gcc"

 `$ echo export RTE_TARGET=x86_64-native-linuxapp-gcc  >> ~/.bashrc`


3.5 set environment variable ONVM_NUM_HUEGPAGES and ONVM_NIC_PCI. 

ONVM_NUM_HUEGPAGES is a variable specifies how many hugepages are reserved by the user, default value of this is 1024, which could be set using: 

 `$ echo export ONVM_NUM_HUEGPAGES=1024 >> ~/.bashrc`

ONVM_NIC_PCI is a variable specifies NIC cards to be binded to DPDK, if user does not set ONVM_NIC_PCI, default is to bind all non-active 10G NIC cards to DPDK, an example for this is as following:

 `$ export ONVM_NIC_PCI=" 07:00.0  07:00.1 "`


3.6 activate the environment status using:

 `$ source ~/.bashrc`


4.  Configure	and	compile	DPDK
-------------
4.1 run `install.sh` in the `scripts` directory to compile dpdk and configure hugepages

The install script will automatically run `scripts/setup_environment.sh`, which configures your local environment. This should be run on every reboot, as it loads the appropraite kernel modules and can bind your NIC to the dpdk driver.

5. Run	HelloWorld	Application
-------------
5.1 enter working directory, and compile the application

 `$ cd ./examples/helloworld/`

 `$make`

5.2 executing the example

  `$ sudo build/helloworld -c 3 -n 1`

if you got the last two line of your result as below, , then you are all set

```
hello from core 1
hello from core 0
```

7. Applying settings after reboot
------------
After a reboot, you can configure your environment again (load kernel modules and bind the NIC) by running `scripts/setup_environment.sh`.
Also, please double check if environment variables are activited using `$echo $Variable` , if not, please go to step 3 to set them. 


Troubleshooting
-------------

### Huge Page Configuration

You can get information about the huge page configuration with:

 ` $ grep -i huge /proc/meminfo`

 You may need to reboot the machine to free memory and reserve the huge pages, if it returns not enough free hugepages or 0 free hugepagse left.

### Binding the NIC to the DPDK Driver
You can check the current status of NIC binding with

  `$ ./tools/dpdk_nic_bind.py  --status`

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

 As you could see, the 10G one is active now, so the next thing is to turn it down

  `$ sudo ifconfig eth2 down`

 now we could check the status again

  `$ ./tools/dpdk_nic_bind.py  --status`

 To bind the 10G to DPDK, notice that only port 0 is wired, so you would like to bind 07:00.0

  `$ sudo ./tools/dpdk_nic_bind.py -b igb_uio 07:00.0`

 Check the status again, if it shows up as following, you are all set

  `$ ./tools/dpdk_nic_bind.py  --status`

 ```
 Network devices using DPDK-compatible driver
 ============================================
 0000:07:00.0 '82599EB 10-Gigabit SFI/SFP+ Network Connection' drv=igb_uio unused=ixgbe

 Network devices using kernel driver
 ===================================
 0000:05:00.0 '82576 Gigabit Network Connection' if=eth0 drv=igb unused=igb_uio *Active*
 0000:07:00.1 '82599EB 10-Gigabit SFI/SFP+ Network Connection' if=eth3 drv=ixgbe unused=igb_uio
 ```
