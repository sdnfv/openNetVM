## Release Notes

#### About:
##### Release Cycle:
We track active development through the `develop` branch and verified
stable releases through the `master` branch.  New releases are created and
tagged with the year and month of their release.  When we tag a release,
we update `master` to have the latest stable code.

##### Versioning:
As of 11/06/2017, we are retiring semantic versioning and will instead
use a date based versioning system.  Now, a release version can look
like `17.11` where the "major" number is the year and the "minor" number
is the month.

#### v18.05 (5/31/18): Bug Fixes, Latency Measurements, and Docker Image
This release adds a feature to the Speed Tester example NF to support latency measurements by using the `-l` flag. Latency is calculated by writing a timestamp into the packet body and comparing this value when the packet is returned to the Speed Tester NF. A sample use case is to run 3 speed tester NFs configured to send in a chain, with the last NF sending back to the first. The first NF can use the `-l` flag to measure latency for this chain. Note that only one NF in a chain should be using the flag since otherwise timestamp information written to the packet will conflict. 

It also makes minor changes to the setup scripts to work better in NSF CloudLab environments.

We now provide a docker container image that can be used to easily run NFs inside containers. See the [Docker Docs](./Docker.md) for more information.

OpenNetVM support has now been integrated into the mainline [mTCP repository](https://github.com/eunyoung14/mtcp).

Finally, we are now adding issues to the GitHub Issue Tracker with the [Good First Issue](https://github.com/sdnfv/openNetVM/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22) label to help others find ways to contribute to the project. Please take a look and contribute a pull request!

An NSF CloudLab template including OpenNetVM 18.05, mTCP, and some basic networking utilities is available here: https://www.cloudlab.us/p/GWCloudLab/onvm-18.05

*No API changes were introduced in this release.*

#### v18.03 (3/27/18): Updated DPDK and preliminary mTCP support
This release updates the DPDK submodule to use version 17.08.  This DPDK update caused breaking changes to its API, so updates have been made to the OpenNetVM manager and example NFs to support this change.

In order to update to the latest version of DPDK you must run:

```
git submodule update --init
```

And then rebuild DPDK using the [install guide](./Install.md) or running these commands:

```
cd dpdk
make clean
make config T=$RTE_TARGET
make T=$RTE_TARGET -j 8
make install T=$RTE_TARGET -j 8
```
(you may need to install the `libnuma-dev` package if you get compilation errors)

This update also includes preliminary support for mTCP-based endpoint NFs. Our OpenNetVM driver has been merged into the [develop branch of mTCP](https://github.com/eunyoung14/mtcp/tree/devel). This allows you to run services like high performance web servers on an integrated platform with other middleboxes.  See the mTCP repository for usage instructions.

Other changes include:
 - Adds a new "Router NF" example which can be used to redirect packets to specific NFs based on their IP. This is currently designed for simple scenarios where a small number of IPs are matched to NFs acting as connection terminating endpoints (e.g., mTCP-based servers). 
 - Bug Fix in ARP NF to properly handle replies based on the ARP OP code.
 - Updated pktgen submodule to 3.49 which works with DPDK 17.08.
 
 An NSF CloudLab template including OpenNetVM 18.03, mTCP, and some basic networking utilities is available here: https://www.cloudlab.us/p/GWCloudLab/onvm-18.03

*No API changes were introduced in this release.*

#### v18.1 (1/31/18): Bug Fixes and Speed Tester improvements
This release includes several bug fixes including:
 - Changed macro and inline function declarations to improve compatibility with 3rd party libraries and newer gcc versions (tested with 4.8 and 5.4)
 - Solved memory leak in SDN flow table example
 - Load Balancer NF now correctly updates MAC address on outgoing packets to backend servers

Improvements:
 - Speed Tester NF now supports a `-c` argument indicating how many packets should be created. If combined with the PCAP replay flag, this parameter controls how many of packets in the trace will be transmitted. A larger packet count may be required when trying to use Speed Tester to saturate a chain of network functions.
 
*No API changes were introduced in this release.*

#### v17.11 (11/16/17): New TX thread architecture, realistic NF examples, better stats, messaging, and more
Since the last official release there have been substantial changes to openNetVM, including the switch to date based versioning mentioned above. Changes include:
 - New TX architecture: previously NFs enqueued packets into a TX ring that was read by TX threads in the manager, which consumed significant CPU resources. 
 By moving TX thread logic to the NF side, ONVM can run with fewer cores, improving efficiency.  NFs can then directly pass packets which saves enqueueing/dequeuing to an extra ring. TX threads still send packets out the NIC, but NFs primarily do packet passing--it is suggested to run the system with at least 1 TX thread to handle outgoing packets. Despite these changes, TX threads can still perform the same work that they did before. If a user would like to run ONVM with TX threads handling all packet passing, they must set `NF_HANDLE_TX` to `0` in `onvm_common.h` 
   - Our tests show this change increases NF transmit speed from 20 Mpps to 41 Mpps with the Speed Tester NF benchmark, while consuming fewer cores.
 - New NFs: we have developed several new sample NFs, including:
   - `examples/ndpi_stats` uses the [nDPI library](https://github.com/ntop/nDPI) for deep packet inspection to determine the protocol of each flow.
   - `examples/flow_tracker` illustrates how to use ONVM's flow table library to track the list of open connections and print information about them.
   - `examples/arp_response` can be used to assign an IP to the NICs managed by openNetVM. The NF is capable of responding to ARP requests. This facilitates NFs that act as connection endpoints, load balancers, etc.
   - `examples/load_balancer` is a layer 3, round-robin load balancer. When a packet arrives the NF checks whether it is from an already existing flow. If not, it creates a new flow entry and assigns it to a destination backend server. This NF uses ARP support to assign an accessible IP to the openNetVM host running the load balancer.
   - [Snort NF](https://github.com/sdnfv/onvm-snort) provides a version of the Snort intrusion detection system ported to openNetVM.
 - [PCAP replay](https://github.com/sdnfv/openNetVM/commit/4b40bdca5117c6a72f57dfa5c622173abfc49483): the Speed Tester NF can now load a packet trace file and use that to generate the packets that it transmits.
 - [NF idle call back](https://github.com/sdnfv/openNetVM/commit/d4bc32aeffeb5f2082cfb978b3860a407c962a93): Traditionally, NFs would wait until the ONVM manager puts packets on their Rx buffer and then calls their packet handler function to process them.  This meant that NFs would sit idle until they have some packets to process.  With this change, NFs can now run at any time even if there are no packets to process.  NFs can provide a callback handler function to be registered with NFLib.  Once this callback handler is registered with NFLib, the function will be run constantly even if there are no packets to be processed.
 - [Web-based stats](https://github.com/sdnfv/openNetVM/commit/b7380020837dcecc32b3fb72e79190c256670e80): the ONVM manager can now display statistics about the active NFs. See `onvm_web/` for more information.
 - [NF--Manager Messaging Interface](https://github.com/sdnfv/openNetVM/commit/125e6dd5e9339b5492723866988edf05ecadcd48): We have expanded the interface between the manager and NFs to allow more flexible message passing. 
 - A multitude of other bug fixes, documentation improvements, etc!

#### v1.1.0 (1/25/17): Refactoring to library, new NFs
This release refactored the code into a proper library, making it easier to include with more advanced NFs. We also added new AES encryption and decryption NFs that operate on UDP packets.

#### v1.0.0 (8/25/16): Refactoring to improve code organization
A big set of commits to clean the structure and simplify onvm source code.
We separated all functions into the main.c of the manager into modules :
 - `onvm_stats` : functions displaying statistics
 - `onvm_pkt` : functions related to packet processing
 - `onvm_nf` : functions related to NFs management.

Each module comes with a header file with commented prototypes. And each c and h file has been "cut" into parts :
 - interfaces, or functions called outside of the module
 - internal functions, the functions called only inside the module and doing all the work
 - helper functions, simple and short functions used many times through the module.

**API Changes:**
 - NFs now need to call functions like `onvm_nflib_*` instead of `onvm_nf_*`.  For example, `onvm_nflib_init` instead of `onvm_nf_init`.  The example NFs have all been updated accordingly.
 - NF `Makefiles` need to be updated to find the path to `onvm_nflib`.


#### 4/24/16: Initial Release
Initial source code release.
