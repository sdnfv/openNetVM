openNetVM Examples
==

Linear NF Chain
--
In this example, we will be setting up a chain of NFs.  The length of the chain is determined by our system's CPU architecture.  Some of the commands used in this example are specific to our system; in the cases where we refer to core lists or number of NFs, please run the [Core Helper Script][cores] to get your numbers.

  1. Determine CPU architecture and running limits:
    - Based on information provided by corehelper script, use the appropriate core information to run the manager and each NF.

            # scripts/corehelper.py
            You supplied 0 arguments, running with flag --onvm

            ===============================================================
                 openNetVM CPU Corelist Helper
            ===============================================================

            ** MAKE SURE HYPERTHREADING IS DISABLED **

            openNetVM requires at least three cores for the manager.
            After the three cores, the manager needs one core for every two NFs'
            TX thread

            Each NF running on openNetVM needs its own core too.  One manager
            TX thread can be used to handle two NFs, but any more becomes
            inefficient.

            Use the following information to run openNetVM on this system:

                    - openNetVM Manager corelist: 0,2,4,6,8,10,12

                    - openNetVM can handle 7 NFs on this system
                            - NF 0: 14
                            - NF 1: 16
                            - NF 2: 18
                            - NF 3: 20
                            - NF 4: 22
                            - NF 5: 24
                            - NF 6: 26

    - Running the script on our machine shows that the system can handle 7 NFs efficiently.  The manager needs three cores and 4 more to handle NFs' TX.
  2. Run Manager:
    - Run the manager in dynamic mode with the following command.  We are using a corelist here to manually pin the manager to specific cores, a portmask to decide which NIC ports to use, and configuring it display manager statistics to stdout:
      - `# onvm/go.sh 0,2,4,6,8,10,12 1 -s stdout`
  3. Start NFs:
    - First, start at most `n-1` simple_forward NFs, where `n` corresponds to the total number of NFs that the system can handle.  This is determined from the `scripts/coremask.py` helper script.  We will only start two NFs for convenience.
    - Simple forward's arguments are core to pin it to, service ID, and
      destination service ID.  The last argument, destination service ID
should be (current_id) + 1 if you want to forward it to the next NF in
the chain.  In this case, we are going to set it to 6, the last NF or
basic_monitor.
      - `# examples/simple_forward/go.sh 14 1 6`
    - Second, start a basic_monitor NF as the last NF in the chain:
      - `# examples/basic_monitor/go.sh 26 6`
  4. Start a packet generator (i.e. [Pktgen-DPDK][pktgen])

Circular NF Chain
--
In this example, we can set up a circular chain of NFs.  Here, traffic does not leave the openNetVM system, rather we are using the speed_tester NF to generate traffic and send it through a chain of NFs.  This example NF can test the speed of the manager's and the NFs' TX threads.

  1. Determine CPU architecture and running limits:
    - Based on information provided by [Core Helper Script][cores], use the appropriate core information to run the manager and each NF.

            # scripts/corehelper.py
            You supplied 0 arguments, running with flag --onvm

            ===============================================================
                 openNetVM CPU Corelist Helper
            ===============================================================

            ** MAKE SURE HYPERTHREADING IS DISABLED **

            openNetVM requires at least three cores for the manager.
            After the three cores, the manager needs one core for every two NFs'
            TX thread

            Each NF running on openNetVM needs its own core too.  One manager
            TX thread can be used to handle two NFs, but any more becomes
            inefficient.

            Use the following information to run openNetVM on this system:

                    - openNetVM Manager corelist: 0,2,4,6,8,10,12

                    - openNetVM can handle 7 NFs on this system
                            - NF 0: 14
                            - NF 1: 16
                            - NF 2: 18
                            - NF 3: 20
                            - NF 4: 22
                            - NF 5: 24
                            - NF 6: 26

    - Running the script on our machine shows that the system can handle 7 NFs efficiently.  The manager needs three cores and 4 more to handle NFs' TX.
  2. Run Manager:
    - Run the manager in dynamic mode with the following command.  We are using a corelist here to manually pin the manager to specific cores, a portmask to decide which NIC ports to use, and configuring it display manager statistics to stdout:
      - `# onvm/go.sh 0,2,4,6,8,10,12 1 -s stdout`
  3. Start NFs:
    - First, start up to n-1 simple_forward NFs.  For simplicity, we'll start one simple_forward NF.
      - The NF will have service ID of 2 and it is pinned to core 16.  It also forwards packets to the NF with service ID 1.
      - `# ./examples/simple_forward/go.sh 16 2 1`
    - Second, start up 1 speed_tester NF and have it forward to service ID 2.
      - `# ./examples/speed_tester/go.sh 14 1 2 -c 16000`
  4. We now have a speed_tester sending packets to service ID 2 who then forwards packets back to service ID 1, the speed_tester.  This is a circular chain of NFs.
  


[cores]: ../scripts/corehelper.py
[pktgen]: https://github.com/pktgen/Pktgen-DPDK
