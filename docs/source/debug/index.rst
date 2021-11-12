Debugging Guide
=====================================

Compile with Debugging Symbols
--------------------------------

When programming NFs or the manager, it is often neccessary to debug. 
By default openNetVM does not compile with debugging symbols but is able to when compiler flags are set. 

For example, you may want to debug `speed tester <https://github.com/sdnfv/openNetVM/blob/master/examples/speed_tester>`_ with gdb.

- The first step is to set the correct environment variables which can be done using the following command:

.. code-block:: bash
    :linenos:
    
    export USER_FLAGS="-g -O0"

- :code:`-g` produces debugging symbols and :code:`-O0` reduces compiler optimizations to the lowest level
- Now navigate to your debug target and run :code:`make`

.. code-block:: bash
    :linenos:
    
    cd examples/speed_tester
    make
    
- The executable now has debugging symbols. Run it in gdb with the necessary args. For example:

.. code-block:: bash
    :linenos:
    
    sudo gdb --args ./build/speed_tester -l 5 -n 3 --proc-type=secondary -- -r 1  -- -d 1
    
- It is now possible to set breakpoints and perform other gdb operations!

**Troubleshooting:**  
If debugging symbols are not found verify that debugging flags are set with :code:`echo $USER_FLAGS` and also try executing the following command in gdb:
- For onvm_mgr:
 
.. code-block:: bash
    :linenos:
    
    file onvm_mgr/x86_64-native-linuxapp-gcc/app/onvm_mgr

- For NFs:
 
.. code-block:: bash
    :linenos:
    
    file build/app/NF_NAME

If for some reason :code:`USER_FLAGS` does not set correctly, it's possible to edit the :code:`Makefile` of your debug target and set the flags manually.
It can be done by adding a line similar to this:

 
.. code-block:: bash
    :linenos:
    
    CFLAGS += -g

Packet capturing
--------------------------------
When working with different packet protocols and tcp related applications it is often needed to look at the packets received/sent by the manager. DPDK provides a dpdk-pdump application that can capture packets to a pcap file.  

To use dpdk-pdump set CONFIG_RTE_LIBRTE_PMD_PCAP=y in dpdk/config/common_base and then recompile dpdk.  

Then execute dpdk-pdump as a secondary application when the manager is running

.. code-block:: bash
    :linenos:
    
    cd dpdk/x86_64-native-linuxapp-gcc
    sudo ./build/app/pdump/dpdk-pdump -- --pdump 'port=0,queue=*,rx-dev=/tmp/rx.pcap'

Full set of options and configurations for dpdk-pdump can be found `here <http://dpdk.org/doc/guides/tools/pdump.html#example>`_.

Possible crash reasons
--
Both primary and secondary dpdk processes must have the exact same hugepage memory mappings to function correctly. This can be an issue when using complex NFs that have a large memory footprint. When using such NFs a memory discrepency occurs between a NF and onvm_mgr, which leads to onvm_mgr crashes.  

The NF/mgr hugepage memory layout discrepency is resolved by using the base virtual address value for onvm_mgr.
Examples of compex NFs: ndpi_stats, onvm_mtcp epserver
  
Example onvm_mgr setup:  

.. code-block:: bash
    :linenos:
    
    ./go.sh -k 3 -n 0xF3 -s stdout
