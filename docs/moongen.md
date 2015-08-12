MoonGen Installation (DPDK-2.0 Version)
===================

#### Welcome to installation memo for [MoonGen](http://arxiv.org/ftp/arxiv/papers/1410/1410.3322.pdf), MoonGen is a "Scriptable High-Speed Packet Generator". 
----------

1. Preparation steps 
===================
Installation steps are assuming that you have already got OpenNetVM installed. If you have already got OpenNetVM installed, please following the steps below for a double check of your system.

1.1 check if you have available hugepages

`$grep -i huge /proc/meminfo`

If ***HugePages_Free*** 's value equals to 0, which means there is no free hugepages available, you probably have to reboot your machine, by `$sudo reboot` to get some released hugepages. 

1.2 check if you have available ports binded to DPDK

`$cd dirctory_of_your_installed_dpdk`

`$./tools/dpdk_nic_bind.py  --status`

if you got the follwing binding information indicating that you have the two 10-Gigabit NIC ports binded with DPDK driver, then you are fine, please jump to step 1.4, otherwise, please jump to step 1.3.

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

1.3 bind the 10G ports to DPDK

1.3.1 load in your uio linux kernel module

`$sudo modprobe uio`

1.3.2 load in your igb_uio, which is in DPDK kernel module, e.g x86_64-native-linuxapp-gcc 

`$sudo insmod x86_64-native-linuxapp-gcc/kmod/igb_uio.ko`

if it showed up as alredy bind, use `$sudo rmmod igb_uio`, and then perform `$sudo insmod x86_64-native-linuxapp-gcc/kmod/igb_uio.ko`. 

1.3.3 bind the 10G ports to DPDK

`$sudo ./tools/dpdk_nic_bind.py -b igb_uio 07:00.0`

`$sudo ./tools/dpdk_nic_bind.py -b igb_uio 07:00.1`

1.4 check if g++ and gcc are updated with version higher than 4.7

`$g++ --version`

`$gcc --version`

if not, please add the repository using:

`$sudo add-apt-repository ppa:ubuntu-toolchain-r/test`

then, to install it use:

`$sudo apt-get update`

`$sudo apt-get install g++-4.7`

and then change the default compiler use update-alternatives:

`$sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.7 40 --slave /usr/bin/g++ g++ /usr/bin/g++-4.7`

`$sudo update-alternatives --config gcc`


2. MoonGen Installation 
===================

2.1 get the resource from github, and checkout the dpdk2.0 branch

`$git clone https://github.com/emmericp/MoonGen`

`$cd MoonGen`

`$git checkout dpdk2.0`

`$sudo git submodule update --init`

2.2 build the resource

`$sudo ./build.sh`

2.3 set up hugetable

`$sudo ./setup-hugetlbfs.sh`

2.4 execute the test, configure the ***quality-of-service-test.lua*** with your destination ip address (ip address for the server you want to sent packets to) in line 60 and line 177, and your source ip address (ip address for the machine you are executing MoonGen on) in line 68 and line 165, and run with command: 

`$sudo ./build/MoonGen  ./examples/quality-of-service-test.lua 0 1`

and if sample log showed up as following, you are fine, please use ***Ctrl+C*** to stop generating packets:

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


















