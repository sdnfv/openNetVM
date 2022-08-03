Packet Generation Using Pktgen
=====================================

**Pktgen** (*Packet Generator*) is a convenient traffic generation tool provided by DPDK and its fast packet processing framework. Pktgen's user-friendly nature and customizable packet generation structure make it a great option for multi-node testing purposes.  

Getting Started
-----------------
In order to get Pktgen up and running, please be sure to follow the step-by-step `Pktgen installation guide <https://github.com/sdnfv/openNetVM-dev/blob/master/tools/Pktgen/README.md>`_.

Transmitting Packets
----------------------
Pktgen in ONVM supports the use of up to 2 ports for packet transmission. Before getting started, make sure that at least one 10Gb NIC is bound to the igb_uio module. You will need two 10Gb NICs bound if you intend to run Pktgen with 2 ports. Both nodes you are using to run Pktgen must have the same NIC ports bound to DPDK.

You can check the status of your NIC port bindings by running :code:`sudo ./dpdk/usertools/dpdk-devbind.py --status` from the home directory of openNetVM. More guidance on how to bind/unbind NICs is provided in the `ONVM Install Guide <https://github.com/sdnfv/openNetVM/blob/master/docs/Install.md#troubleshooting>`_.

The set up of Pktgen will require 2 nodes, Node A and Node B. Node A will run Pktgen, while Node B will run the ONVM manager along with any NFs. 

As noted, both nodes must be bound to the same NIC port(s). 

Running Pktgen with 1 Port 
----------------------------
1. Start the manager on Node B with 1 port: :code:`./go.sh 0,1,2 1 0xF0 -s stdout`
2. On Node A, modify :code:`/tools/Pktgen/OpenNetVM-Scripts/pktgen-config.lua` (the Lua configuration script) to indicate the correct MAC address

.. code-block:: bash
    :linenos:

    pktgen.set_mac("0", "aa:bb:cc:dd:ee:ff")

where :code:`aa:bb:cc:dd:ee:ff` is the port MAC address visible on the manager interface. 
3. Run :code:`./run-pktgen.sh 1` on Node A to start up Pktgen
4. Enter :code:`start all` in the Pktgen command line prompt to being packet transmission
5. To start an NF, open a new shell terminal off of Node B and run the desired NF using the respective commands see our `NF Examples folder <https://github.com/sdnfv/openNetVM/tree/master/examples)>`__ for a list of all NFs and more information).   
***Note:*** *If packets don't appear to be reaching the NF, try restarting the NF with a service ID of 1.*


*To stop the transmission of packets, enter* :code:`stp` *in the Pktgen command line on Node A. To quit Pktgen, use* :code:`quit`.

Running Pktgen with 2 Ports
----------------------------
Before running the openNetVM manager, assure that both Node A and B have the same two 10Gb network interface bound to the DPDK driver (see :code:`./dpdk/usertools`). 

Running Pktgen with 2 ports is very similar to running it with just a single port.

1. Start the manager on Node B with a port mask argument of 3: :code:`./go.sh 0,1,2 3 0xf0 -s stdout` (The `3`  will indicate that 2 ports are to be used by the manager)
2. On Node A, modify `the Lua configuration script <https://github.com/sdnfv/openNetVM/blob/master/tools/Pktgen/openNetVM-Scripts/pktgen-config.lua>`_ to have the correct MAC addresses 

.. code-block:: bash
    :linenos:
    
    pktgen.set_mac("0", "aa:bb:cc:dd:ee:ff");
    pktgen.set_mac("1", "aa:bb:cc:dd:ee:ff");
    
where :code:`0` and :code:`1` refer to the port number and :code:`aa:bb:cc:dd:ee:ff` refer to each of their MAC addresses, as visible on the manager interface. 

3. Run :code:`./run-pktgen.sh 2` on Node A, where :code:`2` indicates the use of 2 ports.
4. To begin transmission, there are a couple of options when presented with the Pktgen command like on Node A:

- :code:`start all` will generate and send packets to both ports set up
- :code:`start 0` will only send generated packets to Port 0
- :code:`start 1` will only send generated packets to Port 1  

*Once one of the above commands is enter, packet transmission will begin*
5. To start an NF, open a new shell terminal off of Node B and run the desired NF using the respective commands (see our `NF Examples folder <https://github.com/sdnfv/openNetVM/tree/master/examples>`__ for a list of all NFs and more information).   
**Note:** *If packets don't appear to be reaching the NF, try restarting the NF with a service ID of 1.*

 *To stop the transmission of packets, enter* :code:`stp` *in the Pktgen command line on Node A. To quit Pktgen, use* :code:`quit`.

Cloudlab Tutorial: Run Bridge NF with Pktgen using 2 Ports 
-------------------------------------------------------------
The demo will be conducted on `CloudLab <https://www.cloudlab.us/login.php>`_ and will follow the basic 2 Node Pktgen steps outlined above.  

ONVM's `Bridge NF <https://github.com/sdnfv/openNetVM/tree/master/examples/bridge>`_ is an example of a basic network bridge. The NF acts a connection between two ports as it sends packets from one port to the other.  

1. Instantiate a Cloudlab experiment using the :code:`2nodes-2links` profile made available by GWCloudLab. (**Note:** *If you do not have access to this profile, instructions on how to create it are outlined below.*)
2. On each node, install `ONVM <https://github.com/sdnfv/openNetVM/blob/master/docs/Install.md>`_ and `Pktgen <https://github.com/sdnfv/openNetVM-dev/blob/master/tools/Pktgen/README.md>`_ as per the instructions provided in the installation guides.    

*Node A will be our designated Pktgen node, while Node B will run the ONVM manager and NFs*

3. Check the NIC port status on each node to ensure that both nodes have the same two 10Gb NICs bound to DPDK. The NIC statuses on each node should look similar to this: 

*From here, we should be able to follow the steps outlined in the above section.*

4. On Node B, start the manager with :code:`./go.sh 0,1,2 3 0xf0 -s stdout`:   
5. On Node A, modify the Lua Pktgen configuration script. Ensure that both MAC addresses in the script match those printed by the ONVM manager:

Manager Display:  

.. code-block:: bash
    :linenos:

    PORTS
    -----
    Port 0: '90:e2:ba:87:6a:f0'       Port 1: '90:e2:ba:87:6a:f1'
    
Pktgen Config File:
 
.. code-block:: bash
    :linenos:
    
    pktgen.set_mac("0", "90:e2:ba:87:6a:f0");
    pktgen.set_mac("1", "90:e2:ba:87:6a:f1"); 

6. On Node A, start Pktgen with :code:`./run-pktgen 2`. Enter :code:`start all` in the command line. This should start sending packets to both Ports 0 and 1.

7. With the ONVM manager running, open a new Node B terminal and start the Bridge NF: :code:`./go.sh 1`.
If you observe the ONVM manager stats, you will notice that, with the Bridge NF, packets sent to Port 0 will be forwarded to and received at Port 1, while packets sent to Port 1 will be received at Port 0. The results should look similar to this:

.. code-block:: bash
    :linenos:
    
    PORTS
    -----
    Port 0: '90:e2:ba:87:6a:f0'       Port 1: '90:e2:ba:87:6a:f1'

    Port 0 - rx: 32904736  (  71713024 pps)   tx: 32903356  (  7173056 pps)
    Port 1 - rx: 32903840  (  71713024 pps)   tx: 32904348  (  7173056 pps)

    NF TAG          IID / SID / CORE        rx_pps / tx_pps         rx_drop / tx_drop             out /   tonf / drop
    ------------------------------------------------------------------------------------------------------------------
    bridge           1  /  1  /  4        14345920 / 14345888           0   /  0            131176508 /    0   /  0

If you'd like to see the Bridge NF working more clearly, you can try sending packets to only one port with either :code:`start 0` or :code:`start 1`. This will allow you to see how the Rx count changes with Bridge NF as more packets arrive.  

*At any point, enter* :code:`stp` *in the Pktgen command line (Node A) if you'd like to stop the transmission of packets. Use* :code:`quit` *to quit Pktgen completely.*


Customizing Packets 
-----------------------
Several user-friendly customization functions are offered through the `Lua configuration file <https://github.com/sdnfv/openNetVM-dev/blob/master/tools/Pktgen/openNetVM-Scripts/pktgen-config.lua>`_.

Setting Packet Size
---------------------
Setting the byte size of packets is important for performance testing. The Lua configuration file allows users to do this:   
:code:`pktgen.set("all", "size", 64);`

Specifying Protocol 
--------------------
If you wish to specify the protocol of each packet, this can be done by modifying the following configuration:   
:code:`pktgen.set_proto("all", "udp");` 
**Note:** Pktgen currently supports TCP/UDP/ICMP protocols.

Number of Packets
-------------------
You may specify the number of packets you want transmit with: :code:`pktgen.set("all", "count", 100000);`
This indicates that you'd like to transmit 100,000 packets.

**All other customization options can be found by entering** :code:`all` **in the Pktgen command line**


Create a 2 Node CloudLab Profile for Pktgen
-------------------------------------------------
1. Log into your `CloudLab <https://www.cloudlab.us/login.php>`_ profile
2. On the User Dashboard, select "Create Experiment Profile" under the "Experiements" tab
3. Enter a name for your profile and choose "Create Topology" nexts to Source Code
4. Drag 2 Bare Metal PCs onto the Site 
5. Link the two nodes by dragging a line between them to connect them. Do this twice. This will ensure that you've created two links between the nodes, allowing for a 1 or 2 port Pkgten setup.
6. Click on one of the nodes to customize it as specified below. Repeat this step for the second node as well. 
    - Hardware Type: :code:`c220g1` or :code:`c220g2`
    - Disk Image: Ubuntu 18-64 STD (or whichever version the latest ONVM version requires)
7. Accept the changes and create your profile
