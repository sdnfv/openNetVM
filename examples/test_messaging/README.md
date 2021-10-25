Test Messaging
==
This is an example NF that acts as a unit test for sending messsages from an NF to itself.
The NF runs the following unit tests:

 1. Send/Receive a single message and check for data correctness
 2. Send/Receive a batch of messages
 3. Send a batch of messages larger than the ring and verify there are no memory pool leaks
 
 Each test is allowed to run for a maximum of 5 seconds.
 
Compilation and Execution
--
```
cd examples
make
cd test_messaging
./go.sh SERVICE_ID
```

Config File Support
--
This NF supports the NF generating arguments from a config file. For
additional reading, see [Examples.md](../../docs/Examples.md)

See `../example_config.json` for all possible options that can be set.