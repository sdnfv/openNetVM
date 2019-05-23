/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2016 George Washington University
 *            2015-2016 University of California Riverside
 *            2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ********************************************************************/


/******************************************************************************

                                 onvm_zk_watch.h

    Header file with functions for receiving Zookeeper callbacks


******************************************************************************/

#ifndef ONVM_ZOOKEEPER_H_
#define ONVM_ZOOKEEPER_H_

#include <inttypes.h>
#include <rte_ether.h>
#include <zookeeper/proto.h>
#include <zookeeper/zookeeper.h>
//#include "cJSON.h"
#include "onvm_init.h"
#include "onvm_nf.h"

#define ZK_SERVER "nimbnode28:2181"

#define ZK_CONNECT_ASYNC 0
#define ZK_CONNECT_BLOCKING 1

#define MGR_NODE_BASE "/manager"
#define MGR_NODE_FMT "/manager/%" PRId64
#define MGR_NODE_STR_FMT "/manager/%s"

#define SERVICE_NODE_BASE "/service"
#define SERVICE_NODE_FMT SERVICE_NODE_BASE "/%" PRIu16 // Format with service ID
#define SERVICE_INSTANCE_FMT SERVICE_NODE_FMT "/%" PRIu64 // Format with service ID + manager ID
#define SERVICE_COUNT_FMT "%" PRIu16

#define NF_NODE_BASE "/nf"
#define NF_SERVICE_BASE NF_NODE_BASE "/%" PRIu16 // format with service id
#define NF_STAT_CHILD_FMT "/nf/%" PRIu16 "/%s" // Format with service ID and child path
#define NF_INSTANCE_FMT NF_SERVICE_BASE "/nf" // format with service id and ZOO_SEQUENCE when creating
#define NF_STAT_FMT "%.5f" // Format with percentage of the ring in use
#define ZK_STAT_UPDATE_FREQ 5 // Update every X times the stats loop is called
#define SCALE_QUEUE_BASE "/scale"
#define SCALE_QUEUE_FMT SCALE_QUEUE_BASE "/%" PRId64  // format with manager id
#define SCALE_DATA_FMT "%"PRIu16

#define MAC_ADDR_FMT "%x:%x:%x:%x:%x:%x"
#define SCALE_RX_USE_MAX 0.70
#define SCALE_TIMEOUT_SEC 10

/**
 * Connect to ZooKeeper
 * ASSUMED that ZK is running on localhost:2791 (callers should ensure that distributed mode is enabled)
 * PARAM mode: if ZK_CONNECT_BLOCKING, then wait for the connection to establish
 * PARAM mgr_address: the MAC address for this instance of onvm so other can communicate with it
 * RETURNS: 0 on success (or already connected), or -1 on connect failure (with errno set),
 */
int onvm_zk_connect(int mode);

/**
 * RETURNS: the 64 bit session ID for this Zookeeper connection
 * RETURNS: 0 if called when not connected
 */
int64_t onvm_zk_client_id(void);

/**
 * Initialize this manager's connection to ZooKeeper
 * Creates an ephemeral node for this manager instance
 * PARAM: MAC address for other instances to send packets here
 */
int onvm_zk_init(const char *port_addr);

/**
 * When a new NF starts, update the stat in ZooKeeper
 * PARAM: service_id is the service of the newly starting NF
 * PARAM: service_count is the new total number of instances of this service running
 * PARAM: instance_id is the instance of the newly starting FNF
 */
int onvm_zk_nf_start(uint16_t service_id, uint16_t service_count, uint16_t instance_id);

/**
 * When a NF stops, update the stat in ZooKeeper
 * PARAM: service_id is the service of the exiting NF
 * PARAM: service_count is the new total number of instances of this service running
 * PARAM: instance_id is the instance ID of the exiting NF
 */
int onvm_zk_nf_stop(uint16_t service_id, uint16_t service_count, uint16_t instance_id);

void onvm_zk_disconnect(void);

/**
 * Look up and see if this service is running somewhere else
 * PARAM: the service ID to lookup
 * RETURNS: the manager ID to send this packet to, or 0 if nowhere to go
 */
int64_t onvm_zk_lookup_service(struct rte_mbuf *pkt, uint16_t service_id, struct ether_addr *dst);

/**
 * Update the stats for this NF. Store the json stats we generate in ZK for all managers
 */
//int onvm_zk_update_nf_stats(uint16_t service_id, uint16_t instance_id, cJSON *stats_json);

#endif
