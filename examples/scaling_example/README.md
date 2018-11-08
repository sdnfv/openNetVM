Scaling Example NF
==
This program showcases how to use NF scaling. It shows how to use the scaling api of onvm and showcases a few simple examples. 

Has 2 modes 

 - First by providing the service and dest without the advanced rings mode, the NF will create a child with the service id of the destination. That child will then create additional children up until it runs out of cores.
 - Second with the advanced rings flag, the NF will create as many children as it can with the same service id and forward packets to dst.

Compilation and Execution
--
```
cd examples
make
cd scaling_example
./go.sh CORELIST SERVICE_ID DST_ID [ADVANCED_RINGS]

OR

sudo ./build/app/scaling -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- -d DST [-a]
```

App Specific Arguments
--
  - `-d DST`: Destination Service ID, functionality depends on mode
  - `-a`: Use advanced rings interface instead of default `packet_handler`
