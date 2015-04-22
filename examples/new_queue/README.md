# New Queue

This application is built off of the simple_mp app and it shows how to create a new queue in DPDK and Hugepages.
This is the model for making custom ring buffers in different
multi-process applications.  The same style of ring_buffer creation is
used in the client_manager application.

### How to run
  - **Primary**: `sudo ./build/simple_mp -c 3 -n 4 --proc-type=primary`
  - **Secondary**: `sudo ./build/simple_mp -c C -n 4 --proc-type=secondary`

### Information
  - new variables: new_ring and new_message_pool
    + new_ring is the new ring_buffer to hold messages as they are
      passed around
    + new_message_pool: this is the mempool used to hold all of the
      messages being passed in the new_ring
  - new functions: fill_ring(void) and get_from_ring(void)
    + fill_ring(void): this function gets a msg buffer from the mempool
      and then crafts a string and stores it there. It will then enqueue that msg.
    + get_from_ring(void): this function checks if there's a message by
      dequeuing the new_ring. If it finds something, it will display it
and then put the buffer back in the mempool
