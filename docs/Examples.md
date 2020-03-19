openNetVM Examples
==

NF Config Files
--
Due to a feature in NFLib, all network functions support launching from a JSON config file that
contains ONVM and DPDK arguments. An example of this can be seen
[here](../examples/example_config.json). In addition, the values
specified in the config file can be overwritten by passing them at the
command line. The general structure for launching an NF from a config file is
`./go.sh -F <CONFIG_FILE.json> <DPDK ARGS> -- <ONVM ARGS> -- <NF ARGS>`.
Any args specified in `<DPDK args>` or `<ONVM ARGS>` will replace the
corresponding args in the config file. **An important note:** If no DPDK
or ONVM args are passed, **but** NF args are required, the `-- --` is
still required. For documentation on developing with config files, see
[NF_Dev](NF_Dev.md)

NF Starting Scripts
--
The example NFs can be started using the `start_nf.sh` script. The script can run any example NF based on the first argument which is the NF name(this is based on the assumption that the name matches the NF folder and the build binary). 
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

            openNetVM requires at least three cores for the manager:
            one for NIC RX, one for statistics, and one for NIC TX.
            For rates beyond 10Gbps it may be necessary to run multiple TX
            or RX threads.

            Each NF running on openNetVM needs its own core too.

            Use the following information to run openNetVM on this system:

                    - openNetVM Manager corelist: 0,1,2

                    - openNetVM can handle 7 NFs on this system
                            - NF 1: 3
                            - NF 2: 4
                            - NF 3: 5
                            - NF 4: 6
                            - NF 5: 7
                            - NF 6: 8
                            - NF 7: 9

    - Running the script on our machine shows that the system can handle 7 NFs efficiently.  
      The manager needs three cores, one for NIC RX, one for statistics, and one for NIC TX.
  2. Run Manager:
    - Run the manager in dynamic mode with the following command.  We are using a corelist here to manually pin the manager to specific cores, a portmask to decide which NIC ports to use, and configuring it display manager statistics to stdout:
      - `# onvm/go.sh 0,1,2 1 0x3F8 -s stdout`
  3. Start NFs:
    - First, start at most `n-1` simple_forward NFs, where `n` corresponds to the total number of NFs that the system can handle.  This is determined from the `scripts/coremask.py` helper script.  We will only start two NFs for convenience.
    - Simple forward's arguments are core to pin it to, service ID, and
      destination service ID.  The last argument, destination service ID
      should be (current_id) + 1 if you want to forward it to the next NF in
      the chain.  In this case, we are going to set it to 6, the last NF or
      basic_monitor.
      - `# examples/start_nf.sh simple_forward 1 -d 6`  
    - Second, start a basic_monitor NF as the last NF in the chain:
      - `# examples/start_nf.sh basic_monitor -d 6`
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

            openNetVM requires at least three cores for the manager:
            one for NIC RX, one for statistics, and one for NIC TX.
            For rates beyond 10Gbps it may be necessary to run multiple TX
            or RX threads.

            Each NF running on openNetVM needs its own core too.

            Use the following information to run openNetVM on this system:

                    - openNetVM Manager corelist: 0,1,2

                    - openNetVM can handle 7 NFs on this system
                            - NF 1: 3
                            - NF 2: 4
                            - NF 3: 5
                            - NF 4: 6
                            - NF 5: 7
                            - NF 6: 8
                            - NF 7: 9

    - Running the script on our machine shows that the system can handle 7 NFs efficiently.  
      The manager needs three cores, one for NIC RX, one for statistics, and one for NIC TX.
  2. Run Manager:
    - Run the manager in dynamic mode with the following command.  We are using a corelist here to manually pin the manager to specific cores, a portmask to decide which NIC ports to use, and configuring it display manager statistics to stdout:
      - `# onvm/go.sh 0,1,2 1 0x3F8 -s stdout`
  3. Start NFs:
    - First, start up to n-1 simple_forward NFs.  For simplicity, we'll start one simple_forward NF.
      - The NF will have service ID of 2.  It also forwards packets to the NF with service ID 1.
      - `# ./examples/start_nf.sh simple_forward 2 -d 1`  
    - Second, start up 1 speed_tester NF and have it forward to service ID 2.
      - `# ./examples/start_nf.sh speed_tester 1 -d 2 -c 16000`
  4. We now have a speed_tester sending packets to service ID 2 who then forwards packets back to service ID 1, the speed_tester.  This is a circular chain of NFs.  


[cores]: ../scripts/corehelper.py
[pktgen]: https://github.com/pktgen/Pktgen-DPDK
