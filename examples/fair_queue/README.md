Fair Queue
==
The Fair Queue NF classifies the packets based on the 5-tuple of Source IP, Source Port, Destination IP, Destination Port, and Protocol into separate queues. [CRC32 hash](https://doc.dpdk.org/api/rte__hash__crc_8h.html#aaf1a10c0c1ee2b63d5a545ee6cb84740) is applied on the 5-tuple and the hash modulo number of queues is used to classify the packets. The packets are dequeued in a round-robin fashion.

Compilation and Execution
--
```
cd examples
make
cd fair_queue
./go.sh SERVICE_ID -d DESTINATION_ID [-n NUM_QUEUES] [-p]
```

App Specific Arguments
--
  - `-n <num_queues>`: Number of queues for the fair queuing system.
  - `-p`: Print per queue statistics
