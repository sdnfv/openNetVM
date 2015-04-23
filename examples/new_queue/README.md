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

### Licensing

'''
BSD LICENSE

Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
Copyright(c) 2015 George Washington University
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in
the documentation and/or other materials provided with the
distribution.
Neither the name of Intel Corporation nor the names of its
contributors may be used to endorse or promote products derived
from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
'''
