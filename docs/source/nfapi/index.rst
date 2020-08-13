NF API
=====================================

OpenNetVM supports 2 modes, by default NFs use the packet_handler callback function that processes packets 1 by 1. The second is the advanced rings mode which gives the NF full control of its rings and allows the developer to do anything they want.

Default Callback mode API init sequence:
------------------------------------------
The `Bridge NF <https://github.com/sdnfv/openNetVM/blob/46bbc962a0ef2ddfd774a7fda798f9ea92b7b116/examples/bridge/bridge.c#L160>`_ is a simple example illustrating this process.

1. First step is initializing the onvm context

.. code-block:: c
    :linenos:
 
    struct onvm_nf_local_ctx *nf_local_ctx;
    nf_local_ctx = onvm_nflib_init_nf_local_ctx();

This configures basic meta data the NF.

2. Start the signal handler

.. code-block:: c
    :linenos:
    
    onvm_nflib_start_signal_handler(nf_local_ctx, NULL);
  
This ensures signals will be caught to correctly shut down the NF (i.e., to notify the manager).

3. Define the function table for the NF
.. code-block:: c
    :linenos:
    
    struct onvm_nf_function_table *nf_function_table;
    nf_function_table = onvm_nflib_init_nf_function_table();
    nf_function_table->pkt_handler = &packet_handler;
    nf_function_table->setup = &nf_setup;

The :code:`pkt_handler` call back will be executed on each packet arrival.  The :code:`setup` function is called only once after the NF is initialized.

4. Initialize ONVM and adjust the argc, argv based on the return value.

.. code-block:: c
    :linenos:
    
    if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG, nf_local_ctx, nf_function_table)) < 0) {
        onvm_nflib_stop(nf_local_ctx);
        if (arg_offset == ONVM_SIGNAL_TERMINATION) {
            printf("Exiting due to user termination\n");
            return 0;
        } else {
            rte_exit(EXIT_FAILURE, "Failed ONVM init\n");
        }
    }

    argc -= arg_offset;
    argv += arg_offset;

This initializes DPDK and notifies the Manager that a new NF is starting.

5. Parse NF specific args

.. code-block:: c
    :linenos:
    
    nf_info = nf_context->nf_info;
    if (parse_app_args(argc, argv, progname) < 0) {
        onvm_nflib_stop(nf_context);
        rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
    }

6. Run the NF

.. code-block:: c
    :linenos:
    
    onvm_nflib_run(nf_local_ctx);

This will cause the NF to enter the run loop, trigger a callback on each new packet.

7. Stop the NF

.. code-block:: c
    :linenos:
    
    onvm_nflib_stop(nf_local_ctx);

Advanced rings API init sequence:
------------------------------------
The scaling NF provides a clear separation of the two modes and can be found `here <https://github.com/sdnfv/openNetVM/blob/master/examples/scaling_example/scaling.c>`_.

1. First step is initializing the onvm context

.. code-block:: c
    :linenos:
    
    struct onvm_nf_local_ctx *nf_local_ctx;
    nf_local_ctx = onvm_nflib_init_nf_local_ctx();

2. Start the signal handler

.. code-block:: c
    :linenos:
    
    onvm_nflib_start_signal_handler(nf_local_ctx, NULL);

3. Contrary to default rings Next we don't need to define the function table

4. Initialize ONVM and adjust the argc, argv based on the return value.

.. code-block:: c
    :linenos:
    
    if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG, nf_local_ctx, NULL)) < 0) {
        onvm_nflib_stop(nf_local_ctx);
        if (arg_offset == ONVM_SIGNAL_TERMINATION) {
            printf("Exiting due to user termination\n");
            return 0;
        } else {
            rte_exit(EXIT_FAILURE, "Failed ONVM init\n");
        }
    }

    argc -= arg_offset;
    argv += arg_offset;

5. Parse NF specific args

.. code-block:: c
    :linenos:
    
    nf_info = nf_context->nf_info;
    if (parse_app_args(argc, argv, progname) < 0) {
        onvm_nflib_stop(nf_context);
        rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
    }

6. To start packet processing run this function to let onvm mgr know that NF is running (instead of `onvm_nflib_run` in default mode

.. code-block:: c
    :linenos:
    
    onvm_nflib_nf_ready(nf_context->nf);

7. Stop the NF

.. code-block:: c
    :linenos:
    
    onvm_nflib_stop(nf_local_ctx);
    
Optional configuration
------------------------

If the NF needs additional NF state data it can be put into the data field, this is NF specific and won't be altered by onvm_nflib functions. This can be defined after the :code:`onvm_nflib_init` has finished 

.. code-block:: c
    :linenos:
    
    nf_local_ctx->nf->data = (void *)rte_malloc("nf_state_data", sizeof(struct custom_state_data), 0);
