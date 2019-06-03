Scaling Example NF
==
This program showcases how to use NF scaling. It shows how to use the ONVM scaling API or use the advanced rings mode and implement your own scaling logic.
The NF will spawn X children (provided by `-n`, defaulting to 1 if arg isn't provided) and send packets to destination NF.    

Has 2 modes:

 - By default using the nflib ring processing, the NF will use the functions provided in the nf_function_table callbacks.
 - Second with the advanced rings flag, the NF will do its own pthread creation, manage the NF rx/tx ring on its own..

Compilation and Execution
--
```
cd examples
make
cd scaling_example
./go.sh SERVICE_ID -d DST_ID [ADVANCED_RINGS]

OR

sudo ./build/app/scaling -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- -d DST [-n] [-a]
```

App Specific Arguments
--
  - `-d DST`: Destination Service ID, functionality depends on mode
  - `-n NUM_CHILDREN`: Sets the number of children for the NF to spawn
  - `-c`: Set spawned children to use shared cpu core allocation
  - `-a`: Use advanced rings interface instead of default `packet_handler`
