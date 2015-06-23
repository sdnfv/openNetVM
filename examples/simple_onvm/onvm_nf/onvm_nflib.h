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


#define ONVM_NF_ACTION_DROP 0  // drop packet
#define ONVM_NF_ACTION_NEXT 1  // to whatever the next action is configured by the SDN controller in the flow table
#define ONVM_NF_ACTION_TONF 2 // send to the NF specified in the argument field (assume it is on the same host)
#define ONVM_NF_ACTION_OUT 3  // send the packet out the NIC port set in the argument field


struct onvm_nf_info {
        uint8_t client_id;

        // What to put there ?
};


/**
 * Initialize the OpenNetVM container Library and start running.
 * This will setup the DPDK EAL as a secondary process, notify the host
 * that there is a new NF, and register the callback used for each new
 * packet. It will then loop forever waiting for packets.
 *
 * @param info
 *   an info struct describing this NF app. Must be from a huge page memzone.
 * @param handler
 *   a pointer to the function that will be called on each received packet.
 * @return
 *   0 on success, or a negative value on error.
 */
int
onvm_nf_init_and_run(struct onvm_nf_info* info, void(*handler)(struct rte_mbuf* pkt));

/**
 * Return a packet to the container library so it can be sent back to the host.
 *
 * @param pkt
 *   a pointer to the completed packet
 * @param action
 *   an NF_ACTION such as forward, drop, etc
 * @param argument
 *   an argument used for some actions
 * @return
 *   0 on success, or a negative value on error.
 */
int
onvm_nf_return_packet(struct rte_mbuf* pkt, uint8_t action, uint16_t argument);

#endif
