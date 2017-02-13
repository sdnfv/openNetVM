[openNetVM][onvm] - with Shared CPUs
==

WARNING
--
This is an **EXPERIMENTAL** version of OpenNetVM. It allows multiple NFs to run on a shared core.  In "normal" OpenNetVM, each NF will poll its RX queue for packets, monopolizing the CPU even if it has a low load.  This branch adds a semaphore-based communication system so that NFs will block when there are no packets available.  The NF Manger will then signal the semaphore once one or more packets arrive.

This code allows you to evaluate resource management techniques for NFs that share cores, however it has not been fully tested and should be considered unstable and unsupported.

For a description of how the code works, see the paper _Flurries: Countless Fine-Grained NFs for Flexible Per-Flow Customization_ by Wei Zhang, Jinho Hwang, Shriram Rajagopalan, K. K. Ramakrishnan, and Timothy Wood, published at _Co-NEXT 16_. Note that this code does not contain the full Flurries system, only the basic support for shared-CPU NFs.

Usage / Known Limitations:
  - All code for sharing CPUs is within `#ifdef INTERRUPT_SEM` blocks. This macro is defined in `onvm/onvm_nflib/onvm_common.h`
  - When enabled, you can run multiple NFs on the same CPU core with much less interference than if they are polling for packets
  - Note that the manager threads all still use polling
  - This code does not provide any particular intelligence for how NFs are scheduled or when they wakeup/sleep
  - Currently ONVM only supports a max of 16 NFs. This can be adjusted by changing macros in `onvm/onvm_nflib/onvm_common.h`
  - Current code has a bug where if multiple NFs start at the exact same time the manager will not correctly assign IDs. You may need to stagger NF startup to avoid this.
  - Killing the manager will not correctly kill all NFs (since they are blocked on semaphore and don't get the shutdown message). You must kill NFs manually with `ctrl-c`.

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
