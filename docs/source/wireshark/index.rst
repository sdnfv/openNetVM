Wireshark and Pdump for Packet Sniffing
==========================================

Wireshark is a must have when debugging any sorts of complex multi node experiments. When running dpdk you can't run Wireshark on the interface but you can view pcap files. Try running the pdump dpdk application to capture packets and then view them in Wireshark.


1. Login using your normal ssh command but append :code:`-X -Y` (when logging from nimbus jumpbox also include :code:`-X -Y`)

2. Install Wireshark 

3. Open it up (depending on terminal/os this might not work for everyone)

.. code-block:: bash
    :linenos:
    
    ssh your_node@cloudlab -X -Y
    sudo apt-get update 
    sudo apt-get install wireshark
    sudo wireshark

Packet capturing
------------------
When working with different packet protocols and TCP related applications it is often needed to look at the packets received/sent by the manager. DPDK provides a dpdk-pdump application that can capture packets to a pcap file.  

To use dpdk-pdump set :code:`CONFIG_RTE_LIBRTE_PMD_PCAP=y` in :code:`dpdk/config/common_base` and then recompile dpdk.  

Then execute dpdk-pdump as a secondary application when the manager is running

.. code-block:: bash
    :linenos:

    cd dpdk/x86_64-native-linuxapp-gcc
    sudo ./build/app/pdump/dpdk-pdump -- --pdump 'port=0,queue=*,rx-dev=/tmp/rx.pcap,tx-dev=/tmp/tx.pcap'

Full set of options and configurations for dpdk-pdump can be found `here <http://dpdk.org/doc/guides/tools/pdump.html#example>`_.

