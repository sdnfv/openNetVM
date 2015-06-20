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
 * init.h - initialization for simple onvm
 ********************************************************************/

#ifndef _INIT_H_
#define _INIT_H_

/*
 * #include <rte_ring.h>
 * #include "args.h"
 */

/*
 * Define a client structure with all needed info, including
 * stats from the clients.
 */
struct client {
        struct rte_ring *rx_q;
        unsigned client_id;
        /* these stats hold how many packets the client will actually receive,
         * and how many packets were dropped because the client's queue was full.
         * The port-info stats, in contrast, record how many packets were received
         * or transmitted on an actual NIC port.
         */
        struct {
                volatile uint64_t rx;
                volatile uint64_t rx_drop;
        } stats;
};

extern struct client *clients;

/* the shared port information: port numbers, rx and tx stats etc. */
extern struct port_info *ports;

extern struct rte_mempool *pktmbuf_pool;
extern uint8_t num_clients;
extern unsigned num_sockets;
extern struct port_info *ports;

int init(int argc, char *argv[]);

#endif /* ifndef _INIT_H_ */
