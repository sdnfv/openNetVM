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
 * init.c - initialization for simple onvm
 ********************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/queue.h>
#include <errno.h>
#include <stdarg.h>
#include <inttypes.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_byteorder.h>
#include <rte_atomic.h>
#include <rte_launch.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_debug.h>
#include <rte_ring.h>
#include <rte_log.h>
#include <rte_mempool.h>
#include <rte_memcpy.h>
#include <rte_mbuf.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_fbk_hash.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_errno.h>

#include "shared/common.h"
#include "onvm_mgr/args.h"
#include "onvm_mgr/init.h"

#define MBUFS_PER_CLIENT 1536
#define MBUFS_PER_PORT 1536
#define MBUF_CACHE_SIZE 512
#define MBUF_OVERHEAD (sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define RX_MBUF_DATA_SIZE 2048
#define MBUF_SIZE (RX_MBUF_DATA_SIZE + MBUF_OVERHEAD)

#define NF_INFO_SIZE sizeof(struct onvm_nf_info)
#define NF_INFO_CACHE 8

#define RTE_MP_RX_DESC_DEFAULT 512
#define RTE_MP_TX_DESC_DEFAULT 512
#define CLIENT_QUEUE_RINGSIZE 128

#define NO_FLAGS 0

/* The mbuf pool for packet rx */
struct rte_mempool *pktmbuf_pool;

/* mbuf pool for nf info structs */
struct rte_mempool *nf_info_pool;

/* ring buffer for new clients coming up */
struct rte_ring *nf_info_queue;

/* array of info/queues for clients */
struct client *clients = NULL;

/* the port details */
struct port_info *ports = NULL;

struct client_tx_stats *clients_stats;

/**
 * Initialise the mbuf pool for packet reception for the NIC, and any other
 * buffer pools needed by the app - currently none.
 */
static int
init_mbuf_pools(void) {
        const unsigned num_mbufs = (MAX_CLIENTS * MBUFS_PER_CLIENT) \
                        + (ports->num_ports * MBUFS_PER_PORT);

        /* don't pass single-producer/single-consumer flags to mbuf create as it
         * seems faster to use a cache instead */
        printf("Creating mbuf pool '%s' [%u mbufs] ...\n",
                        PKTMBUF_POOL_NAME, num_mbufs);
        pktmbuf_pool = rte_mempool_create(PKTMBUF_POOL_NAME, num_mbufs,
                        MBUF_SIZE, MBUF_CACHE_SIZE,
                        sizeof(struct rte_pktmbuf_pool_private), rte_pktmbuf_pool_init,
                        NULL, rte_pktmbuf_init, NULL, rte_socket_id(), NO_FLAGS);

        return (pktmbuf_pool == NULL); /* 0  on success */
}

/**
 * Set up a mempool to store nf_info structs
 */
static int
init_client_info_pool(void)
{
        /* don't pass single-producer/single-consumer flags to mbuf
         * create as it seems faster to use a cache instead */
        printf("Creating mbuf pool '%s' ...\n", _NF_MEMPOOL_NAME);
        nf_info_pool = rte_mempool_create(_NF_MEMPOOL_NAME, MAX_CLIENTS,
                        NF_INFO_SIZE, NF_INFO_CACHE,
                        0, NULL, NULL, NULL, NULL, rte_socket_id(), NO_FLAGS);

        return (nf_info_pool == NULL); /* 0 on success */
}

/**
 * Initialise an individual port:
 * - configure number of rx and tx rings
 * - set up each rx ring, to pull from the main mbuf pool
 * - set up each tx ring
 * - start the port and report its status to stdout
 */
static int
init_port(uint8_t port_num) {
        /* for port configuration all features are off by default */
        const struct rte_eth_conf port_conf = {
                .rxmode = {
                        .mq_mode = ETH_MQ_RX_RSS
                }
        };
        const uint16_t rx_rings = 1, tx_rings = MAX_CLIENTS;
        const uint16_t rx_ring_size = RTE_MP_RX_DESC_DEFAULT;
        const uint16_t tx_ring_size = RTE_MP_TX_DESC_DEFAULT;

        uint16_t q;
        int retval;

        printf("Port %u init ... ", (unsigned)port_num);
        fflush(stdout);

        /* Standard DPDK port initialisation - config port, then set up
         * rx and tx rings */
        if ((retval = rte_eth_dev_configure(port_num, rx_rings, tx_rings,
                &port_conf)) != 0)
                return retval;

        for (q = 0; q < rx_rings; q++) {
                retval = rte_eth_rx_queue_setup(port_num, q, rx_ring_size,
                                rte_eth_dev_socket_id(port_num),
                                NULL, pktmbuf_pool);
                if (retval < 0) return retval;
        }

        for (q = 0; q < tx_rings; q++) {
                retval = rte_eth_tx_queue_setup(port_num, q, tx_ring_size,
                                rte_eth_dev_socket_id(port_num),
                                NULL);
                if (retval < 0) return retval;
        }

        rte_eth_promiscuous_enable(port_num);

        retval  = rte_eth_dev_start(port_num);
        if (retval < 0) return retval;

        printf("done: \n");

        return 0;
}

/**
 * Set up the DPDK rings which will be used to pass packets, via
 * pointers, between the multi-process server and client processes.
 * Each client needs one RX queue.
 */
static int
init_shm_rings(void) {
        unsigned i;
        unsigned socket_id;
        const char * rq_name;
        const char * tq_name;
        const unsigned ringsize = CLIENT_QUEUE_RINGSIZE;

        // use calloc since we allocate for all possible clients
        // ensure that all fields are init to 0 to avoid reading garbage
        // TODO plopreiato, move to creation when a NF starts
        clients = rte_calloc("client details",
                MAX_CLIENTS, sizeof(*clients), 0);
        if (clients == NULL)
                rte_exit(EXIT_FAILURE, "Cannot allocate memory for client program details\n");

        for (i = 0; i < MAX_CLIENTS; i++) {
                /* Create an RX queue for each client */
                socket_id = rte_socket_id();
                rq_name = get_rx_queue_name(i);
                tq_name = get_tx_queue_name(i);
                clients[i].client_id = i;
                clients[i].rx_q = rte_ring_create(rq_name,
                                ringsize, socket_id,
                                RING_F_SP_ENQ | RING_F_SC_DEQ); /* single prod, single cons */
                clients[i].tx_q = rte_ring_create(tq_name,
                                ringsize, socket_id,
                                RING_F_SP_ENQ | RING_F_SC_DEQ); /* single prod, single cons */

                if (clients[i].rx_q == NULL)
                        rte_exit(EXIT_FAILURE, "Cannot create rx ring queue for client %u\n", i);

                if (clients[i].tx_q == NULL)
                        rte_exit(EXIT_FAILURE, "Cannot create tx ring queue for client %u\n", i);
        }
        return 0;
}

/**
 * Allocate a rte_ring for newly created NFs
 */
static int
init_info_queue(void)
{
        nf_info_queue = rte_ring_create(
                _NF_QUEUE_NAME,
                MAX_CLIENTS,
                rte_socket_id(),
                RING_F_SC_DEQ); // MP enqueue (default), SC dequeue

        if (nf_info_queue == NULL)
                rte_exit(EXIT_FAILURE, "Cannot create nf info queue\n");

        return 0;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint8_t port_num, uint32_t port_mask) {
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
        uint8_t portid, count, all_ports_up, print_flag = 0;
        struct rte_eth_link link;

        printf("\nChecking link status");
        fflush(stdout);
        for (count = 0; count <= MAX_CHECK_TIME; count++) {
                all_ports_up = 1;
                for (portid = 0; portid < port_num; portid++) {
                        if ((port_mask & (1 << ports->id[portid])) == 0)
                                continue;
                        memset(&link, 0, sizeof(link));
                        rte_eth_link_get_nowait(ports->id[portid], &link);
                        /* print link status if flag set */
                        if (print_flag == 1) {
                                if (link.link_status)
                                        printf("Port %d Link Up - speed %u "
                                                "Mbps - %s\n", ports->id[portid],
                                                (unsigned)link.link_speed,
                                (link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
                                        ("full-duplex") : ("half-duplex\n"));
                                else
                                        printf("Port %d Link Down\n",
                                                (uint8_t)ports->id[portid]);
                                continue;
                        }
                        /* clear all_ports_up flag if any link down */
                        if (link.link_status == 0) {
                                all_ports_up = 0;
                                break;
                        }
                }
                /* after finally printing all link status, get out */
                if (print_flag == 1)
                        break;

                if (all_ports_up == 0) {
                        printf(".");
                        fflush(stdout);
                        rte_delay_ms(CHECK_INTERVAL);
                }

                /* set the print_flag if all ports up or timeout */
                if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
                        print_flag = 1;
                        printf("done\n");
                }
        }
}

/**
 * Main init function for the multi-process server app,
 * calls subfunctions to do each stage of the initialisation.
 */
int
init(int argc, char *argv[]) {
        int retval;
        const struct rte_memzone *mz;
        uint8_t i, total_ports;

        /* init EAL, parsing EAL args */
        retval = rte_eal_init(argc, argv);
        if (retval < 0)
                return -1;
        argc -= retval;
        argv += retval;

        /* get total number of ports */
        total_ports = rte_eth_dev_count();

        /* set up array for client tx data */
        mz = rte_memzone_reserve(MZ_CLIENT_INFO, sizeof(*clients_stats),
                                rte_socket_id(), NO_FLAGS);
        if (mz == NULL)
                rte_exit(EXIT_FAILURE, "Cannot reserve memory zone for client information\n");
        memset(mz->addr, 0, sizeof(*clients_stats));
        clients_stats = mz->addr;

        /* set up ports info */
        ports = rte_malloc(MZ_PORT_INFO, sizeof(*ports), 0);
        if (ports == NULL)
                rte_exit(EXIT_FAILURE, "Cannot allocate memory for ports details\n");

        /* parse additional, application arguments */
        retval = parse_app_args(total_ports, argc, argv);
        if (retval != 0)
                return -1;

        /* initialise mbuf pools */
        retval = init_mbuf_pools();
        if (retval != 0)
                rte_exit(EXIT_FAILURE, "Cannot create needed mbuf pools\n");

        /* initialise client info pool */
        retval = init_client_info_pool();
        if (retval != 0) {
                rte_exit(EXIT_FAILURE, "Cannot create client info mbuf pool: %s\n", rte_strerror(rte_errno));
        }

        /* now initialise the ports we will use */
        for (i = 0; i < ports->num_ports; i++) {
                retval = init_port(ports->id[i]);
                if (retval != 0)
                        rte_exit(EXIT_FAILURE, "Cannot initialise port %u\n",
                                        (unsigned)i);
        }

        check_all_ports_link_status(ports->num_ports, (~0x0));

        /* initialise the client queues/rings for inter-eu comms */
        init_shm_rings();

        /* initialise a queue for newly created NFs */
        init_info_queue();

        return 0;
}
