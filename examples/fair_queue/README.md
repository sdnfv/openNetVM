Fair Queue
==
The Fair Queue NF categorises the packets based on the 5-tuple of Source IP, Source Port, Destination IP, Destination Port and Protocol into separate queues while applying a token bucket filter on each queue. The token bucket parameters can be configured using the config.json file.<br>

The rate of production of tokens is specified in MBps while the depth of the token bucket is specified in bytes. An example for the config file is as follows:

````
"num_queues": 2,
"queues: {
    "queue1": {
        "rate": 1000,
        "depth": 10000
    },
    "queue2": {
        "rate": 500,
        "depth": 10000
    }
}
````
Compilation and Execution
--
```
cd examples
make
cd fair_queue
./go.sh SERVICE_ID -d DESTINATION_ID [-f CONFIG_FILE]
```

App Specific Arguments
--
  - `-f <config_file>`: Token bucket configuration for queues.
