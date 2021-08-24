MoonGen Installation (DPDK-19.5 Version)
===================

#### Welcome to installation memo for [MoonGen](http://scholzd.github.io/MoonGen/install.html). MoonGen is a "Scriptable High-Speed Packet Generator". 
----------

## 1. Preparation steps
===================

Installation steps are assuming that you already have [openNetVM installed](./Install.md). Follow the steps below to double check your system configuration.

### 1.1 Check for available hugepages

`$ grep -i huge /proc/meminfo`

If the ***HugePages_Free*** 's value equals 0, which means there are no free hugepages available, there may be a few reasons why: 
- The manager crashed, but an NF(s) is still running.
    - In this case, either kill them manually by hitting Ctrl+C or run `$ sudo pkill NF_NAME` for every NF that you have ran.
- The manager and NFs are not running, but something crashed without freeing hugepages.
    - To fix this, please run `$ sudo rm -rf /mnt/huge/*` to remove all files that contain hugepage data.
- The above two cases are not met, something weird is happening:
    - A reboot might fix this problem and free memory: `$ sudo reboot`

### 1.2 Check NIC ports are bound to DPDK

`$ cd dirctory_of_your_installed_dpdk`

`$ ./usertools/dpdk-devbind.py  --status`

If you got the follwing similar binding information indicating that you have the two 10-Gigabit NIC ports bound with the DPDK driver, jump to step 1.4. Otherwise, please jump to step 1.3.

```
Network devices using DPDK-compatible driver
============================================
0000:07:00.0 '82599EB 10-Gigabit SFI/SFP+ Network Connection' drv=igb_uio unused=ixgbe
0000:07:00.1 '82599EB 10-Gigabit SFI/SFP+ Network Connection' if=eth3 drv=ixgbe unused=ixgbe

Network devices using kernel driver
===================================
0000:05:00.0 '82576 Gigabit Network Connection' if=eth0 drv=igb unused=igb_uio *Active*
0000:05:00.1 '82576 Gigabit Network Connection' if=eth0 drv=igb unused=igb_uio *Active*
```

### 1.3 Bind the 10G ports to DPDK

An example of incorrect bindings is as follows: 

```
Network devices using DPDK-compatible driver
============================================
<none>

Network devices using kernel driver
===================================
0000:05:00.0 '82576 Gigabit Network Connection' if=eth0 drv=igb unused=igb_uio *Active*
0000:05:00.1 '82576 Gigabit Network Connection' if=eth1 drv=igb unused=igb_uio
0000:07:00.0 '82599EB 10-Gigabit SFI/SFP+ Network Connection' if=eth2 drv=ixgbe unused=igb_uio *Active*
0000:07:00.1 '82599EB 10-Gigabit SFI/SFP+ Network Connection' if=eth3 drv=ixgbe unused=igb_uio
```

In our example above, we see two 10G capable NIC ports that we could use with description '82599EB 10-Gigabit SFI/SFP+ Network Connection'.

One of the two NIC ports, 07:00.0, is active shown by the *Active* at the end of the line. Since the Linux Kernel is currently using that port, network interface eth2, we will not be able to use it with openNetVM. We must first disable the network interface in the Kernel, and then proceed to bind the NIC port to the DPDK Kernel module, igb_uio:

`$ sudo ifconfig eth2 down`

Rerun the status command, ./usertools/dpdk-devbind.py --status, to see that it is not active anymore. Once that is done, proceed to bind the NIC port to the DPDK Kenrel module:

`$ sudo ./usertools/dpdk-devbind.py -b igb_uio 07:00.0`

Check the status again, `$ ./usertools/dpdk-devbind.py --status`, and assure the output is similar to our example below:

```
Network devices using DPDK-compatible driver
============================================
0000:07:00.0 '82599EB 10-Gigabit SFI/SFP+ Network Connection' drv=igb_uio unused=ixgbe

Network devices using kernel driver
===================================
0000:05:00.0 '82576 Gigabit Network Connection' if=eth0 drv=igb unused=igb_uio *Active*
0000:05:00.1 '82576 Gigabit Network Connection' if=eth1 drv=igb unused=igb_uio
0000:07:00.1 '82599EB 10-Gigabit SFI/SFP+ Network Connection' if=eth3 drv=ixgbe unused=igb_uio
```

1.4 check if g++ and gcc are updated with version higher than 4.8

`$ g++ --version`

`$ gcc --version`

if not, please add the repository using:

`$ sudo add-apt-repository ppa:ubuntu-toolchain-r/test`

Then, to install it use:

`$ sudo apt-get update`

`$ sudo apt-get install g++-4.8`

and then change the default compiler to use update-alternatives:

`$ sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.8 40 --slave /usr/bin/g++ g++ /usr/bin/g++-4.8`

`$ sudo update-alternatives --config gcc`

Install other dependencies with:

```
$ sudo apt-get install -y build-essential cmake linux-headers-`uname -r` pciutils libnuma-dev
$ sudo apt install cmake
$ sudo apt install libtbb2
```


## 2. MoonGen Installation
===================

### 2.1 Get the resource from github, and checkout the dpdk19.5 branch

`$ git clone https://github.com/emmericp/MoonGen` 

`$ cd MoonGen`

`$ git checkout dpdk-19.05`

`$ sudo git submodule update --init`

### 2.2 Build the resource

`$ sudo ./build.sh`

### 2.3 Set up hugetable

`$ sudo ./setup-hugetlbfs.sh`

### 2.4 Execute the test
Configure the ***quality-of-service-test.lua*** with your destination ip address (ip address for the server you want to sent packets to) and your source ip address (ip address for the machine you are executing MoonGen on), and run with command: 

`$ sudo ./build/MoonGen  ./examples/quality-of-service-test.lua 0 1`

and if the sample log outputs the following, your configutation is correct. Use ***Ctrl+C*** to stop generating packets:

```
wenhui@nimbnode16:~/MoonGen$ sudo ./build/MoonGen ./examples/quality-of-service-test.lua 0 0
Found 2 usable ports:
Ports 0: 00:1B:21:80:6A:04 (82599EB 10-Gigabit SFI/SFP+ Network Connection)
Ports 1: 00:1B:21:80:6A:05 (82599EB 10-Gigabit SFI/SFP+ Network Connection)
Waiting for ports to come up...
Port 0 (00:1B:21:80:6A:04) is up: full-duplex 10000 MBit/s
1 ports are up.
[Port 42] Sent 1460655 packets, current rate 1.46 Mpps, 1495.62 MBit/s, 1729.32 MBit/s wire rate.
[Port 43] Sent 97902 packets, current rate 0.10 Mpps, 100.18 MBit/s, 115.83 MBit/s wire rate.
[Port 42] Sent 2926035 packets, current rate 1.47 Mpps, 1500.54 MBit/s, 1735.00 MBit/s wire rate.
[Port 43] Sent 195552 packets, current rate 0.10 Mpps, 99.98 MBit/s, 115.61 MBit/s wire rate.
[Port 42] Sent 4391415 packets, current rate 1.47 Mpps, 1500.54 MBit/s, 1735.00 MBit/s wire rate.

......

^C[Port 42] Sent 15327522 packets with 1961922816 bytes payload (including CRC).
[Port 42] Sent 1.465371 (StdDev 0.000010) Mpps, 1500.540084 (StdDev 0.009860) MBit/s, 1734.999472 (StdDev 0.011401) MBit/s wire rate on average.
[Port 43] Sent 1020600 packets with 130636800 bytes payload (including CRC).
[Port 43] Sent 0.097653 (StdDev 0.000017) Mpps, 99.996549 (StdDev 0.017340) MBit/s, 115.621010 (StdDev 0.020049) MBit/s wire rate on average.
PMD: ixgbe_dev_tx_queue_stop(): Tx Queue 1 is not empty when stopping.
PMD: ixgbe_dev_tx_queue_stop(): Could not disable Tx Queue 0
PMD: ixgbe_dev_tx_queue_stop(): Could not disable Tx Queue 1
Background traffic: Average -9223372036854775808, Standard Deviation 0, Quartiles -9223372036854775808/-9223372036854775808/-9223372036854775808
Foreground traffic: Average -9223372036854775808, Standard Deviation 0, Quartiles -9223372036854775808/-9223372036854775808/-9223372036854775808
```


















