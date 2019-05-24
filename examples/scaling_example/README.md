Scaling Example NF
==
This program showcases how to use NF scaling. It shows how to use the scaling api of onvm and showcases a few simple examples. 

Has 2 modes 

 - First by providing the service and dest without the advanced rings mode, the NF will create a child with the service id of the destination. That child will then create additional children up until it reaches the set children count.
 - Second with the advanced rings flag, the NF will create as many children as set, with the service id provided and forward packets to set dest.

Compilation and Execution
--
```
cd examples
make
cd scaling_example
./go.sh SERVICE_ID -d DST_ID [ADVANCED_RINGS]

OR

sudo ./build/app/scaling -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- -d DST [-a]
```

App Specific Arguments
--
  - `-d DST`: Destination Service ID, functionality depends on mode
  - `-n NUM_CHILDREN`: Sets the number of children for the NF to spawn
  - `-c`: Set spawned children to use shared cpu core allocation
  - `-a`: Use advanced rings interface instead of default `packet_handler`
