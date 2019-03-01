# Release Notes

### About:
#### Release Cycle:
We track active development through the `develop` branch and verified
stable releases through the `master` branch.  New releases are created and
tagged with the year and month of their release.  When we tag a release,
we update `master` to have the latest stable code.

#### Versioning:
As of 11/06/2017, we are retiring semantic versioning and will instead
use a date based versioning system.  Now, a release version can look
like `17.11` where the "major" number is the year and the "minor" number
is the month.

## v19.02 (2/19): Manager Assigned NF Cores, Global Launch Script, DPDK 18.11 Update, Web Stats Overhaul, Load Generator NF, CI (Internal repo only), minor improvements and bug fixes
This release adds several new features and changes how the onvm_mgr and NFs start. A CloudLab template is available with the latest release here: https://www.cloudlab.us/p/GWCloudLab/onvm

Note: This release makes important changes in how NFs are run and assigned to cores. 

Performance: We are aware of some performance irregularities with this release. For example, the first few times a Basic Monitor NF is run we achieve only ~8 Mpps on a CloudLab Wisconsin c220g2 server. After starting and stopping the NF several times, the performance rises to the expected 14.5 Mpps.

### Manager Assigned NF Cores:
NFs no longer require a CORE_LIST argument to start, the manager now does core assignment based on the provided core bitmask argument. 

NFs now go through the dpdk init process on a default core (currently 0) and then launch a pthread for its main loop, which using the DPDK `rte_thread_set_affinity()` function is affinized to a core obtained from the Manager. 

The core info is maintained in a memzone and the Manager keeps track of what cores are used, by how many NFs, and if the cores are reserved as dedicated. The Manager always selects the core with the fewest NFs unless a flag is used when starting an NF.

**Usage:**

New Manager arguments:
  * Hexadecimal bitmask, which tells the onvm_mgr which cores are available for NFs to run on.

The manager now must be run with a command like:
```sh
cd onvm
#./go.sh CORE_LIST PORT_BITMASK NF_CORE_BITMASK -s LOG_MODE
./go.sh 0,1,2,3 0x3 0xF0 -s stdout

```
With this command the manager runs on cores 0-3, uses ports 1 and 2 (since `0x3` is binary `0b11`), and will start NFs on cores 4-7 (since `0xF0` is binary `0b11110000`)

New Network Functions arguments: 
  * `-m` manual core decision mode, NF runs on the core supplied by the `-l` argument if available. If the core is busy or not enabled then returns an error and doesn't start the NF.
  * `-s` shared core mode, this will allow multiple NFs to run on the same core. Generally this should be avoided to prevent performance problems. By default, each core is dedicated to a single NF.
  
These arguments can be set as `ONVM_ARGS` as detailed below.

**API Additions:**
 - `int onvm_threading_core_affinitize(int core)` - Affinitizes the calling thread to a new core. This is used both internally and by the advanced rings NFs to change execution cores.  

### Global Launch Script
The example NFs can be started using the `start_nf.sh` script. The script can run any example NF based on the first argument which is the NF name (this is based on the assumption that the name matches the NF folder and the build binary). This removes the need to maintain a separate `go.sh` script for each NF but requires some arguments to be explicitly specified.

The script has 2 modes:
 - Simple
    ```sh
    ./start_nf.sh NF_NAME SERVICE_ID (NF_ARGS)
    ./start_nf.sh speed_tester 1 -d 1
    ```
  - Complex
    ```sh
    ./start_nf.sh NF_NAME DPDK_ARGS -- ONVM_ARGS -- NF_ARGS
    ./start_nf.sh speed_tester -l 4 -- -s -r 6 -- -d 5
    ```
*All the NF directories have a symlink to `examples/go.sh` file which allows to omit the NF name argument when running the NF from its directory:*
```sh
    cd speed_tester && ./go.sh 1 -d 1
    cd speed_tester && ./go.sh -l 4 -- -s -r 6 -- -d 5
```


### DPDK 18.11 Update
DPDK submodule no longer points to our fork, we now point to the upstream DPDK repository. This is because mTCP requirements for DPDK have relaxed and they no longer need to have additional patches on top of it.  

Also updates Pktgen to 3.6.5 to remain compatible with DPDK v18.11
The dpdk update involves:
- Adds NIC ring RSS hashing functions adjustments
- Adds NIC ring file descriptor size alignment

Run this to ensure the submodule is up to date:
```sh
git submodule sync
git submodule update --init
```

### Web Stats Overhaul 
Adds a new event logging system which is used for port initialization and NF starting, ready, and stopping events. In the future, this could be used for more complex logging such as service chain based events and for core mappings.

Also contains a complete rewrite of the web frontend. The existing code which primarily used jquery has been rewritten and expanded upon in React, using Flow for type checking rather than a full TypeScript implementation. This allows us to maintain application state across pages and to restore graphs to the fully updated state when returning to a graph from a different page.

Please note that **CSV download has been removed** with this update as storing this much ongoing data negatively impacts application performance. This sort of data collection would be best implemented via grepping or some similar functionality from onvm console output.

### Load Generator NF
Adds a Load Generator NF, which sends packets at a specified rate and size, measures tx and rx throughput (pps) and latency. The load_generator NF continuously allocates and sends new packets of a defined size and at a defined rate using the `callback_handler` function. The max value for the `-t` pkt_rate argument for this NF will depend on the underlying architecture, for best performance increase it up until you see the NF starting to drop packets.

Example usage with a chain of load_generator <-> simple_forward:
```sh
cd examples/load_generator
./go.sh 1 -d 2 -t 4000000 

cd examples/simple_forward
./go.sh 2 -d 1
```

Example NF output:
```
Time elapsed: 24.50

Tx total packets: 98001437
Tx packets sent this iteration: 11
Tx rate (set): 4000000
Tx rate (average): 3999999.33
Tx rate (current): 3999951.01

Rx total packets: 94412314
Rx rate (average): 3853506.69
Rx rate (current): 4000021.01
Latency (current mean): 4.38 us
```


### CI (Internal repo only)
Adds continuous integration to the internal repo. CI will automatically run when a new PR is created or when keyword `@onvm` is mentioned in a pr comment. CI currently reports the linter output and the Speed Tester NF performance. This will be tested internally and extended to support the public repo when ready.  

To achieve this a Flask server listens to events from github, currently only the `openNetVM-dev` repo is setup for this. In the future we plan to expand this functionality to the public `openNetVM` repo.  

### Bug Fixes
 - Fix how NF_STOPPED message is sent/processed. This fixes the double shutdown bug (observed in mTCP applications), the fast ctrl-c exit bug and the invalid arguments bug. In all of those cases memory would get corrupted, this bug fix resolves these cases.  
 - Add out of bounds checks for NF service ids. Before we were not handling cases when a new NF service id exceeded the MAX_SERVICES value or when launching a new NF would exceed the NF_SERVICE_COUNT_MAX value for the given service id.  
 - Fix the Speed Tester NF to properly exit when passed an invalid MAC addr argument.  

## v18.11 (11/18): Config files, Multithreading, Better Statistics, and bug fixes
This release adds several new features which cause breaking API changes to existing NFs.  NFs must be updated to support the new API required for multithreading support. A CloudLab template is available with the latest release here: https://www.cloudlab.us/p/GWCloudLab/onvm

 ### Multithreading:
NFs can now run multiple threads, each with its own set of rings for receiving and transmitting packets. NFs can either start new threads themselves or the NF Manager can send a message to an NF to cause it to scale up.

**Usage:**
To make an NF start another thread, run the `onvm_nflib_scale(struct onvm_nf_scale_info *scale_info)` function with a struct holding all the information required to start the new NF thread. This can be used to replicate an NF's threads for scalability (all with same service ID), or to support NFs that require several threads performing different types of processing (thus each thread has its own service ID). More info about the multithreading can be found in `docs/NF_Dev.md`. Example use of multithreading NF scaling can be seen in the `scaling_example` NF.

**API Changes:**
The prior code relied on global data structures that do not work in a multithreaded environment. As a result, many of the APIs have been refactored to take an `onvm_nf_info` structure, instead of assuming it is available as a global variable.
 - `int onvm_nflib_init(int argc, char *argv[], const char *nf_tag);` -> 
`int onvm_nflib_init(int argc, char *argv[], const char *nf_tag, struct onvm_nf_info **nf_info_p)`
 - `void onvm_nflib_stop(void)` -> `void onvm_nflib_stop(struct onvm_nf_info *nf_info)` 
- `int onvm_nflib_return_pkt(struct rte_mbuf* pkt)` -> 
`int onvm_nflib_return_pkt(struct onvm_nf_info *nf_info, struct rte_mbuf* pkt)`
- `int pkt_handler_func(struct rte_mbuf* pkt, struct onvm_pkt_meta* action)` -> 
`int pkt_handler_func(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta, __attribute__ ((unused)) struct onvm_nf_info *nf_info)`
- `int callback_handler_func(void)` -> `int callback_handler_func(__attribute__ ((unused)) struct onvm_nf_info *nf_info)`
- Any existing NFs will need to be modified to support this updated API. Generally this just requires adding a reference to the `onvm_nf_info` struct in the API calls.

NFs also must adjust their Makefiles to include the following libraries:
```
CFLAGS += -I$(ONVM)/lib
LDFLAGS += $(ONVM)/lib/$(RTE_TARGET)/lib/libonvmhelper.a -lm
```

**API Additions:**
 - `int onvm_nflib_scale(struct onvm_nf_scale_info *scale_info)` launches another NF based on the provided config
 - `struct onvm_nf_scale_info * onvm_nflib_get_empty_scaling_config(struct onvm_nf_info *parent_info)` for getting a basic empty scaling config
 - `struct onvm_nf_scale_info * onvm_nflib_inherit_parent_config(struct onvm_nf_info *parent_info)` for getting a scaling config with the same functionality (e.g., service ID) as the parent NF
 - `void onvm_nflib_set_setup_function(struct onvm_nf_info* info, setup_func setup)` sets the setup function to be automatically executed once before an NF enters the main packet loop  

### Stats Display  
The console stats display has been improved to aggregate stats when running multiple NFs with the same service ID and to add two additional modes: verbose for all stats in human readable format and raw stats dump for easy script parsing. The NF TX stat has been updated to also include tonf traffic.


**Usage:**   
- For normal mode no extra steps are required  
- For verbose mode run the manager with `-v` flag  
- For raw stats dump use the `-vv` flag  

### Config File Support:
ONVM now supports JSON config files, which can be loaded through the API provided in `onvm_config_common.h`. This allows various settings of either the ONVM manager or NFs to be set in a JSON config file and loaded into code, as opposed to needing to be passed in via the command line.

**Usage:**
 - All example NFs now support passing DPDK and ONVM arguments in a config file by using the `-F config.json` flag when running an NF executable or a `go.sh` script.  See `docs/examples.md` for more details.

**API Changes:**
- `nflib.c` was not changed from an NF-developer standpoint, but it was modified to include a check for the `-F` flag, which indicates that a config file should be read to launch an NF.

**API Additions:**
 - `cJSON* onvm_config_parse_file(const char* filename)`: Reads a JSON config and stores the contents in a cJSON struct. For further reference on cJSON, see its [documentation](https://github.com/DaveGamble/cJSON).
- `int onvm_config_create_nf_arg_list(cJSON* config, int* argc, char** argv[])`: Given a cJSON struct and pointers to the original command line arguments, generate a new `argc` and `argv` using the config file values.

### Minor improvements

  - **Return packets in bulk**: Adds support for returning packets in bulk instead of one by one by using  `onvm_nflib_return_pkt_bulk`. Useful for functions that buffer a group of packets before returning them for processing or for NFs that create batches of packets in the fast path. *No breaking API changes.*
  - **Updated `corehelper.py` script**: Fixed the `scripts/corehelper.py` file so that it correctly reports recommended core usage instructions. The script assumes a single CPU socket system and verifies that hyperthreading is disabled.
  - **Adjusted default number of TX queues**: Previously, the ONVM manager always started `MAX_NFS` transmit queues on each NIC port. This is unnecessary and causes a problem with SR-IOV and NICs with limited queue support. Now the manager creates one queue per TX thread.
  - Bug fixes were made to [prevent a crash](https://github.com/sdnfv/openNetVM/commit/087891d9fea3b3ab011254dd405ef9e708d2e43d) of `speed_tester` during allocation of packets when there are no free mbufs and to [fix an invalid path](https://github.com/sdnfv/openNetVM/commit/a7978304914670ae9dfd2e3571af21ec7ed29013) causing an error when attempting to use Pktgen with the `run-pktgen.sh` script. Additionally, a few [minor documentation edits](https://github.com/sdnfv/openNetVM/commit/6005be5724552cda3f84b84e39cdc7bee846194c) were made.


## v18.05 (5/31/18): Bug Fixes, Latency Measurements, and Docker Image
This release adds a feature to the Speed Tester example NF to support latency measurements by using the `-l` flag. Latency is calculated by writing a timestamp into the packet body and comparing this value when the packet is returned to the Speed Tester NF. A sample use case is to run 3 speed tester NFs configured to send in a chain, with the last NF sending back to the first. The first NF can use the `-l` flag to measure latency for this chain. Note that only one NF in a chain should be using the flag since otherwise timestamp information written to the packet will conflict. 

It also makes minor changes to the setup scripts to work better in NSF CloudLab environments.

We now provide a docker container image that can be used to easily run NFs inside containers. See the [Docker Docs](./Docker.md) for more information.

OpenNetVM support has now been integrated into the mainline [mTCP repository](https://github.com/eunyoung14/mtcp).

Finally, we are now adding issues to the GitHub Issue Tracker with the [Good First Issue](https://github.com/sdnfv/openNetVM/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22) label to help others find ways to contribute to the project. Please take a look and contribute a pull request!

An NSF CloudLab template including OpenNetVM 18.05, mTCP, and some basic networking utilities is available here: https://www.cloudlab.us/p/GWCloudLab/onvm-18.05

*No API changes were introduced in this release.*

## v18.03 (3/27/18): Updated DPDK and preliminary mTCP support
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

## v18.1 (1/31/18): Bug Fixes and Speed Tester improvements
This release includes several bug fixes including:
 - Changed macro and inline function declarations to improve compatibility with 3rd party libraries and newer gcc versions (tested with 4.8 and 5.4)
 - Solved memory leak in SDN flow table example
 - Load Balancer NF now correctly updates MAC address on outgoing packets to backend servers

Improvements:
 - Speed Tester NF now supports a `-c` argument indicating how many packets should be created. If combined with the PCAP replay flag, this parameter controls how many of packets in the trace will be transmitted. A larger packet count may be required when trying to use Speed Tester to saturate a chain of network functions.
 
*No API changes were introduced in this release.*

## v17.11 (11/16/17): New TX thread architecture, realistic NF examples, better stats, messaging, and more
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

## v1.1.0 (1/25/17): Refactoring to library, new NFs
This release refactored the code into a proper library, making it easier to include with more advanced NFs. We also added new AES encryption and decryption NFs that operate on UDP packets.

## v1.0.0 (8/25/16): Refactoring to improve code organization
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


## 4/24/16: Initial Release
Initial source code release.
