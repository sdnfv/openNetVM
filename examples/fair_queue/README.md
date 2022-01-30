Fair Queue
==
The Fair Queue NF simulates fair queueing by classifying packets based on the 5-tuple of Source IP, Source Port, Destination IP, Destination Port, and Protocol into separate queues and dequeuing the packets in a round robin fashion.

Contributed by [Rohit MP](https://gist.github.com/rohit-mp) from NITK

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
