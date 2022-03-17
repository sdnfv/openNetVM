Scaling Example NF
==
This program showcases how to use NF scaling. It shows how to use the ONVM scaling API or use the advanced rings mode and implement your own scaling logic.
The NF will spawn X children (provided by `-n`, defaulting to 1 if arg isn't provided) and send packets to destination NF.    

This NF can be run in 2 modes:

 - **Callback Mode**: This mode will be used by default. In callback mode, the NF will use the functions provided in the `nf_function_table` callbacks. `nf_function_table->pkt_handler` is set to a default function for forwarding packets to the destination NF provided by `-d`. 
 - **Advanced Rings Mode**: This mode will be used when the advanced rings flag, `-a`, is set. The NF will do its own pthread creation and manage the NF rx/tx ring on its own. In advanced rings mode, the default action for processing packets in `thread_main_loop` is to forward packets to the destination NF provided by `-d`. 

Compilation and Execution
--
```
cd examples
make
cd scaling_example
./go.sh SERVICE_ID -d DST_ID [NUM_CHILDREN] [ADVANCED_RINGS]

OR

sudo ./build/app/scaling -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- -d DST [-n] [-a]
```

App Specific Arguments
--
  - `-d DST`: Destination Service ID, functionality depends on mode
  - `-n NUM_CHILDREN`: Sets the number of children for the NF to spawn
  - `-c`: Set spawned children to use shared cpu core allocation
  - `-a`: Use advanced rings interface instead of default `packet_handler`
