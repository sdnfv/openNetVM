[openNetVM][onvm] - with Shared CPUs
==

WARNING
--
This is an **EXPERIMENTAL** version of OpenNetVM. It allows multiple NFs to run on a shared core.  In "normal" OpenNetVM, each NF will poll its RX queue for packets, monopolizing the CPU even if it has a low load.  This branch adds a semaphore-based communication system so that NFs will block when there are no packets available.  The NF Manger will then signal the semaphore once one or more packets arrive.

This code allows you to evaluate resource management techniques for NFs that share cores, however it has not been fully tested and should be considered unstable and unsupported.

For a description of how the code works, see the papers
  - _Flurries: Countless Fine-Grained NFs for Flexible Per-Flow Customization_ by Wei Zhang, Jinho Hwang, Shriram Rajagopalan, K. K. Ramakrishnan, and Timothy Wood, published at _Co-NEXT 16_. Note that this code does not contain the full Flurries system, only the basic support for shared-CPU NFs.
  - _NFVnice: Dynamic Backpressure and Scheduling for NFV Service Chains. Sameer G. Kulkarni, Wei Zhang, Jinho Hwang, Shriram Rajagopalan, K.K. Ramakrishnan, Timothy Wood, Mayutan Arumaithurai, and Xiaoming Fu, published at ACM SIGCOMM, August 2017. 
  - _REINFORCE: Achieving Efficient Failure Resiliency for Network Function Virtualization based Services. Sameer G. Kulkarni, Guyue Liu, K. K. Ramakrishnan, Mayutan Arumaithurai, Timothy Wood, and Xiaoming Fu, published at ACM CoNEXT, 2018.

Usage / Known Limitations:
  - All code for sharing CPUs is within `#ifdef INTERRUPT_SEM` blocks. This macro is defined in `onvm/onvm_nflib/onvm_common.h`
  - When enabled, you can run multiple NFs on the same CPU core with much less interference than if they are polling for packets
  - Note that the manager threads all still use polling
  - Currently ONVM only supports a max of 16 NFs. This can be adjusted by changing macros in `onvm/onvm_nflib/onvm_common.h`
  - NFVnice: Enables rate-cost proportional CPU sharing using the Cgroups for each NF and enables backpressure across NF chains to provide load-assisted NF scheduling. All the code for NFvnice is within the `#ifdef ENABLE_NF_BACKPRESSURE` and `#ifdef ENABLE_CGROUPS_FEATURE` blocks. These macro are defined in `onvm/onvm_nflib/onvm_common.h` and requires the `INTERRUPT_SEM` macro to be enabled. 
  - Reinforce: Enables NF failure resiliency: failover on local node by creating primary and standby NFs on same node and provides remote NF chain wide failure resiliency on remote nodes. All the code for Resiliency is within `#ifdef ENABLE_NFV_RESL` blocks. This macro is defined in `onvm/onvm_nflib/onvm_common.h` and requires the `INTERRUPT_SEM` macro to be enabled.
  - This code update is based on the [openNetVM-v18.11] release. For more details on other associated feature updates please refer to the [openNetVM-v18.11] release notes.

WARNING
--

About
--
openNetVM is a high performance NFV platform based on [Intel DPDK][dpdk] and [Docker][docker] containers.  openNetVM is SDN-enabled, allowing the network controller to provide rules that dictate what network functions need to process each packet flow.

openNetVM is an open source version of the NetVM platform described in our [NSDI 2014 paper][nsdi04], released under the [BSD][license] license.

Installing
--
To install openNetVM, please see the [openNetVM Installation][install] guide for a thorough walkthrough.

Using openNetVM
--
openNetVM can be used in a variety of ways.  To get started with some examples, please see the [Example Uses][examples] guide

Creating NFs
--
The [NF Development][nfs] guide will provide what you need to start create your own NFs.

Dockerize NFs
--
NFs can be run inside docker containers, with the NF being automatically or hand started. For more informations, see our [Docker guide][docker-nf].



[onvm]: http://sdnfv.github.io/onvm/
[license]: LICENSE
[dpdk]: http://dpdk.org
[docker]: https://www.docker.com/
[nsdi04]: http://faculty.cs.gwu.edu/~timwood/papers/14-NSDI-netvm.pdf
[install]: docs/Install.md
[examples]: docs/Examples.md
[nfs]: docs/NF_Dev.md
[docker-nf]: docs/Docker.md
[openNetVM-v18.11]: https://github.com/sdnfv/openNetVM-dev/releases/tag/v18.11
