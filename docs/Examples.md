openNetVM Examples
==

Linear NF Chain
--
In this example, we will be setting up a chain of NFs.  The length of the chain is determined by our system's CPU architecture.  Some of the commands used in this example are specific to our system; in the cases where we refer to core lists or number of NFs, please run the [Core Helper Script][cores] to get your numbers.

  1. Determine CPU architecture and running limits:
    - Based on information provided by coremask script, use the appropriate core information to run the manager and each NF.

            # scripts/coremask.py
            You supplied 0 arguments, running with flag --onvm

			===============================================================
			                 openNetVM CPU Coremask Helper
			===============================================================

			openNetVM requires at least three cores for the manager.
			After three cores, the manager can have more too.  Since
			NFs need a thread for TX, the manager can have many dedicated
			TX threads which would all need a dedicated core.

			Each NF running on openNetVM needs its own core too.  One manager
			TX thread can be used to handle two NFs, but any more becomes
			inefficient.

			Use the following information to run openNetVM on this system:

			        - openNetVM Manager -- coremask: 0x7f

			        - CPU Layout permits 7 NFs with these coremasks:
			                + NF 0 -- coremask: 0x2000
			                + NF 1 -- coremask: 0x1000
			                + NF 2 -- coremask: 0x800
			                + NF 3 -- coremask: 0x400
			                + NF 4 -- coremask: 0x200
			                + NF 5 -- coremask: 0x100
			                + NF 6 -- coremask: 0x80

    - Running the script on our machine shows that the system can handle 7 NFs efficiently.  The manager needs three cores and 4 more to handle NFs' TX.
  2. Run Manager:
    - Run the manager in dynamic mode with the following command.  We are using a corelist here to manually pin the manager to specific cores and a portmask to decide which NIC ports to use:
      - `# onvm/go.sh 0,2,4,6,8,10,12 1`
  3. Start NFs:
    - First, start `n-1` simple_forward NFs, where `n` corresponds to the total number of NFs that the system can handle.  This is determined from the `scripts/coremask.py` helper script.
      - `# examples/simple_forward/go.sh 14 1`
    - Second, start a basic_monitor NF as the last NF in the chain:
      - `# examples/basic_monitor/go.sh 26 6`
  4. Start a packet generator (i.e. [Pktgen-DPDK][pktgen])

Circular NF Chain
--
In this example, we can set up a circular chain of NFs.  Here, traffic does not leave the openNetVM system, rather we are using the speed_tester NF to generate traffic and send it through a chain of NFs.  This example NF can test the speed of the manager's and the NFs' TX threads.

  1. Determine CPU architecture and running limits:
    - Based on information provided by [Core Helper Script][cores], use the appropriate core information to run the manager and each NF.

            # scripts/coremask.py
            You supplied 0 arguments, running with flag --onvm

			===============================================================
			                 openNetVM CPU Coremask Helper
			===============================================================

			openNetVM requires at least three cores for the manager.
			After three cores, the manager can have more too.  Since
			NFs need a thread for TX, the manager can have many dedicated
			TX threads which would all need a dedicated core.

			Each NF running on openNetVM needs its own core too.  One manager
			TX thread can be used to handle two NFs, but any more becomes
			inefficient.

			Use the following information to run openNetVM on this system:

			        - openNetVM Manager -- coremask: 0x7f

			        - CPU Layout permits 7 NFs with these coremasks:
			                + NF 0 -- coremask: 0x2000
			                + NF 1 -- coremask: 0x1000
			                + NF 2 -- coremask: 0x800
			                + NF 3 -- coremask: 0x400
			                + NF 4 -- coremask: 0x200
			                + NF 5 -- coremask: 0x100
			                + NF 6 -- coremask: 0x80

    - Running the script on our machine shows that the system can handle 7 NFs efficiently.  The manager needs three cores and 4 more to handle NFs' TX.
  2. Run Manager:
    - Run the manager in dynamic mode with the following command.  We are using a corelist here to manually pin the manager to specific cores and a portmask to decide which NIC ports to use:
      - `# onvm/go.sh 0,2,4,6,8,10,12 1`
  3. Start NFs:
    - First, start up to n-1 simple_forward NFs.  For simplicity, we'll start one simple_forward NF.
      - The NF will have service ID of 2 and it is pinned to core 16.  It also forwards packets to the NF with service ID 1.
      - `# examples/simple_forward/simple_forward/x86_64-native-linuxapp-gcc/forward -l 16 -n 3 --proc-type=auto -- -r 2 -- -d 1`
    - Second, start up 1 speed_tester NF and have it forward to service ID 2.
      - `# examples/speed_tester/speed_tester/x86_64-native-linuxapp-gcc/speed_tester -l 14 -n 3 --proc-type=auto -- -r 1 -- -d 2`
  4. We now have a speed_tester sending packets to service ID 2 who then forwards packets back to service ID 1, the speed_tester.  This is a circular chain of NFs.


[cores]: https://github.com/sdnfv/openNetVM/blob/master/scripts/coremask.py
[pktgen]: https://github.com/pktgen/Pktgen-DPDK