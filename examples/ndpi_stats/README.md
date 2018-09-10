NDPI Stats
==
NDPI Stats is an  NF which inspects incoming packets and detects their protocol.  
If -d dest_nf is not specified the NF will send packets OUT similar to basic_monitor.

Using nDPI Library
--
The nDPI source code can be found here: https://github.com/ntop/nDPI.  
Using the nDPI library with ndpi_util.c, ndpi_util.h from the provided nDPI reader example.  
The current generated stats are a simplified version of ndpiReader example from the above repo.

Complex NF note
--
When using this NF depending on the hugepage setup onvm_mgr can crash. This occurs because nDPI is a large library which changes the memory footprint of the NF and creates a discrepancy between onvm_mgr and NF memory layout.  

Both primary and secondary dpdk processes must have the exact same hugepage memory mappings to function correctly. The NF/mgr hugepage memory layout discrepency is resolved by using the base virtual address value for onvm_mgr.  

Example onvm_mgr setup:  
```./go.sh 0,1,2,3 3 -v 0x7f000000000 -s stdout```

Compilation and Execution
--

To use this NF install nDPI following these directions: https://github.com/ntop/nDPI.  
Set NDPI_HOME env variable to the nDPI root directory.

```
cd examples
make
cd ndpi_stats
./go.sh CORELIST SERVICE_ID [FILE_NAME] [DEST_NF]

OR

sudo ./build/ndpi_stats -l CORELIST -n NUM_MEMORY_CHANNELS --proc-type=secondary -- -r SERVICE_ID -- [-w FILE_NAME] [-d DEST_NF]
```

App Specific Arguments
--
  - `-w <file_name>`: result file name to write to.
  - `-d <nf_id>`: OPTIONAL destination NF to send packets to
