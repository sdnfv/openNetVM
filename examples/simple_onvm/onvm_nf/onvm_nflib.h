/*********************************************************************
 *                     openNetVM
 *       https://github.com/sdnfv/openNetVM
 *
 *  Copyright 2015 George Washington University
 *            2015 University of California Riverside
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 * onvm_nflib.c - client lib NF for simple onvm
 ********************************************************************/


#ifndef _ONVM_NFLIB_H_
#define _ONVM_NFLIB_H_
#include <rte_mbuf.h>


#define ONVM_NF_ACTION_DROP 0 // drop packet
#define ONVM_NF_ACTION_NEXT 1 // to whatever the next action is configured by the SDN controller in the flow table
#define ONVM_NF_ACTION_TONF 2 // send to the NF specified in the argument field (assume it is on the same host)
#define ONVM_NF_ACTION_OUT 3  // send the packet out the NIC port set in the argument field

extern struct rte_ring *tx_ring; //this ring is used to pass packets from NFlib to NFmgr

struct onvm_nf_info {
        uint8_t client_id;

        // What to put there ?
};


/**
 * Initialize the OpenNetVM container Library.
 * This will setup the DPDK EAL as a secondary process, and notify the host
 * that there is a new NF.
 *
 * @argc
 *   The argc argument that was given to the main() function.
 * @argv
 *   The argv argument that was given to the main() function
 * @param info
 *   An info struct describing this NF app. Must be from a huge page memzone.
 * @return
 *   On success, the number of parsed arguments, which is greater or equal to
 *   zero. After the call to onvm_nf_init(), all arguments argv[x] with x < ret
 *   may be modified and should not be accessed by the application.,
 *   On error, a negative value .
 */
int
onvm_nf_init(int argc, char *argv[], struct onvm_nf_info* info);


/**
 * Run the OpenNetVM container Library.
 * This will register the callback used for each new packet. It will then
 * loop forever waiting for packets.
 *
 * @param info
 *   an info struct describing this NF app. Must be from a huge page memzone.
 * @param handler
 *   a pointer to the function that will be called on each received packet.
 * @return
 *   0 on success, or a negative value on error.
 */
int
onvm_nf_run(struct onvm_nf_info* info, void(*handler)(struct rte_mbuf* pkt, struct onvm_pkt_action* action));

#endif
