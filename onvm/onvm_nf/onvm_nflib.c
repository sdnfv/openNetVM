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


#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_log.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_ring.h>
#include <rte_debug.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_string_fns.h>

#include "common.h"
#include "onvm_nflib.h"
#include "onvm_sc_common.h"

/* Number of packets to attempt to read from queue */
#define PKT_READ_SIZE  ((uint16_t)32)

/* ring used to place new nf_info struct */
static struct rte_ring *nf_info_ring;

/* rings used to pass packets between NFlib and NFmgr */
static struct rte_ring *tx_ring, *rx_ring;

/* shared data from server. We update statistics here */
static volatile struct client_tx_stats *tx_stats;

/* Shared data for client info */
extern struct onvm_nf_info *nf_info;

/* Shared pool for all clients info */
static struct rte_mempool *nf_info_mp;

/* User-given NF Client ID (defaults to manager assigned) */
static uint8_t initial_client_id = NF_NO_ID;

/* True as long as the NF should keep processing packets */
static uint8_t keep_running = 1;

/* Shared data for default service chain*/
static struct onvm_service_chain *default_chain;

/*
 * Print a usage message
 */
static void
usage(const char *progname) {
        printf("Usage: %s [EAL args] -- [-n <client_id>]\n\n", progname);
}

/*
 * Parse the library arguments.
 */
static int
parse_nflib_args(int argc, char *argv[]) {
        const char *progname = argv[0];
        int c;

        opterr = 0;
        while ((c = getopt (argc, argv, "n:")) != -1)
                switch (c) {
                case 'n':
                        initial_client_id = (uint8_t) strtoul(optarg, NULL, 10);
                        break;
                case '?':
                        usage(progname);
                        if (optopt == 'n')
                                fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                        else if (isprint(optopt))
                                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                        else
                                fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                        return -1;
                default:
                        return -1;
                }
        return optind;
}

/**
 * CALLED BY NF:
 * Create a new nf_info struct for this NF
 * Pass a unique tag for this NF
 */
static struct onvm_nf_info *
ovnm_nf_info_init(const char *tag)
{
        void *mempool_data;
        struct onvm_nf_info *info;

        if (rte_mempool_get(nf_info_mp, &mempool_data) < 0) {
                rte_exit(EXIT_FAILURE, "Failed to get client info memory");
        }

        if (mempool_data == NULL) {
                rte_exit(EXIT_FAILURE, "Client Info struct not allocated");
        }

        info = (struct onvm_nf_info*) mempool_data;
        info->client_id = initial_client_id;
        info->status = NF_WAITING_FOR_ID;
        info->tag = tag;

        return info;
}

/**
 * CALLED BY NF:
 * Sets this NF to not runnings and exits with EXIT_SUCCESS
 */
void
onvm_nf_stop(void) {
        rte_exit(EXIT_SUCCESS, "Done.");
}

/**
 * CALLED BY NF:
 * Initialises everything we need
 */
int
onvm_nf_init(int argc, char *argv[], const char *nf_tag) {
        const struct rte_memzone *mz;
	const struct rte_memzone *mz_scp;
        struct rte_mempool *mp;
        int retval_eal, retval_parse;
	struct onvm_service_chain **scp;

        if ((retval_eal = rte_eal_init(argc, argv)) < 0)
                return -1;
        argc -= retval_eal; argv += retval_eal;

        if ((retval_parse = parse_nflib_args(argc, argv)) < 0)
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");

        /* Lookup mempool for nf_info struct */
        nf_info_mp = rte_mempool_lookup(_NF_MEMPOOL_NAME);
        if (nf_info_mp == NULL)
                rte_exit(EXIT_FAILURE, "No Client Info mempool - bye\n");

        /* Initialize the info struct */
        nf_info = ovnm_nf_info_init(nf_tag);

        mp = rte_mempool_lookup(PKTMBUF_POOL_NAME);
        if (mp == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get mempool for mbufs\n");

        mz = rte_memzone_lookup(MZ_CLIENT_INFO);
        if (mz == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get tx info structure\n");
        tx_stats = mz->addr;

	mz_scp = rte_memzone_lookup(MZ_SCP_INFO);
	if (mz_scp == NULL) 
		rte_exit(EXIT_FAILURE, "Cannot get service chain info structre\n");
	scp = mz_scp->addr;
	default_chain = *scp;

	onvm_sc_print(default_chain);
	/*just for test, this  should be called by the thread which contacts to the controller.*/
	#if 0
	int retval;
	onvm_sc_append_entry(default_chain, ONVM_NF_ACTION_NEXT, 3);
	onvm_sc_print(default_chain);
	retval = onvm_sc_set_entry(default_chain, 0, ONVM_NF_ACTION_OUT, 40);
	if (retval == 1) {
		printf("Change service chain entry fails\n");
	}
	onvm_sc_print(default_chain);
	#endif

        nf_info_ring = rte_ring_lookup(_NF_QUEUE_NAME);
        if (nf_info_ring == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get nf_info ring");

        /* Put this NF's info struct onto queue for manager to process startup */
        if (rte_ring_enqueue(nf_info_ring, nf_info) < 0) {
                rte_mempool_put(nf_info_mp, nf_info); // give back mermory
                rte_exit(EXIT_FAILURE, "Cannot send nf_info to manager");
        }

        /* Wait for a client id to be assigned by the manager */
        RTE_LOG(INFO, APP, "Waiting for manager to assign an ID...\n");
        for (; nf_info->status == (uint8_t)NF_WAITING_FOR_ID ;) {
                sleep(1);
        }

        /* This NF is trying to declare an ID already in use. */
        if (nf_info->status == NF_ID_CONFLICT) {
                rte_mempool_put(nf_info_mp, nf_info);
                rte_exit(NF_ID_CONFLICT, "Selected ID already in use. Exiting...\n");
        } else if(nf_info->status == NF_NO_IDS) {
                rte_mempool_put(nf_info_mp, nf_info);
                rte_exit(NF_NO_IDS, "There are no ids available for this NF\n");
        } else if(nf_info->status != NF_STARTING) {
                rte_mempool_put(nf_info_mp, nf_info);
                rte_exit(EXIT_FAILURE, "Error occurred during manager initialization\n");
        }
        RTE_LOG(INFO, APP, "Using ID %d\n", nf_info->client_id);

        /* Now, map rx and tx rings into client space */
        rx_ring = rte_ring_lookup(get_rx_queue_name(nf_info->client_id));
        if (rx_ring == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get RX ring - is server process running?\n");

        tx_ring = rte_ring_lookup(get_tx_queue_name(nf_info->client_id));
        if (tx_ring == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get TX ring - is server process running?\n");

        /* Tell the manager we're ready to recieve packets */
        nf_info->status = NF_RUNNING;

        RTE_LOG(INFO, APP, "Finished Process Init.\n");
        return (retval_eal + retval_parse);
}

/**
 * Called for SIGINT, or ^C
 * Tells the main loop it's time to exit and clean up
 */
static void
handle_signal(int sig)
{
        if (sig == SIGINT)
                keep_running = 0;
}

/**
 * CALLED BY NF:
 * Application main function - loops through
 * receiving and processing packets. Never returns
 */
int
onvm_nf_run(struct onvm_nf_info* info, int(*handler)(struct rte_mbuf* pkt, struct onvm_pkt_meta* meta)) {
        void *pkts[PKT_READ_SIZE];
        struct onvm_pkt_meta* meta;

        printf("\nClient process %d handling packets\n", info->client_id);
        printf("[Press Ctrl-C to quit ...]\n");

        /* Listen for ^C so we can exit gracefully */
        signal(SIGINT, handle_signal);

        for (; keep_running;) {
                uint16_t i, j, nb_pkts = PKT_READ_SIZE;
                void *pktsTX[PKT_READ_SIZE];
                int tx_batch_size = 0;
                int ret_act;

                /* try dequeuing max possible packets first, if that fails, get the
                 * most we can. Loop body should only execute once, maximum */
                while (nb_pkts > 0 &&
                                unlikely(rte_ring_dequeue_bulk(rx_ring, pkts, nb_pkts) != 0))
                        nb_pkts = (uint16_t)RTE_MIN(rte_ring_count(rx_ring), PKT_READ_SIZE);

                if(nb_pkts == 0) {
                        continue;
                }
                /* Give each packet to the user proccessing function */
                for (i = 0; i < nb_pkts; i++) {
                        meta = onvm_get_pkt_meta((struct rte_mbuf*)pkts[i]);
                        ret_act = (*handler)((struct rte_mbuf*)pkts[i], meta);
                        /* NF returns 0 to return packets or 1 to buffer */
                        if(likely(ret_act == 0)) {
                                pktsTX[tx_batch_size++] = pkts[i];
                        }
                        else {
                                tx_stats->tx_buffer[info->client_id]++;
                        }
                }

                if (unlikely(tx_batch_size > 0 && rte_ring_enqueue_bulk(tx_ring, pktsTX, tx_batch_size) == -ENOBUFS)) {
                        tx_stats->tx_drop[info->client_id] += tx_batch_size;
                        for (j = 0; j < tx_batch_size; j++) {
                                rte_pktmbuf_free(pktsTX[j]);
                        }
                } else {
                        tx_stats->tx[info->client_id] += tx_batch_size;
                }
        }

        nf_info->status = NF_STOPPED;

        /* Put this NF's info struct back into queue for manager to ack shutdown */
        nf_info_ring = rte_ring_lookup(_NF_QUEUE_NAME);
        if (nf_info_ring == NULL) {
                rte_mempool_put(nf_info_mp, nf_info); // give back mermory
                rte_exit(EXIT_FAILURE, "Cannot get nf_info ring for shutdown");
        }

        if (rte_ring_enqueue(nf_info_ring, nf_info) < 0) {
                rte_mempool_put(nf_info_mp, nf_info); // give back mermory
                rte_exit(EXIT_FAILURE, "Cannot send nf_info to manager for shutdown");
        }
        return 0;
}

/*
 * Return a buffered packet.
 */
int
onvm_nf_return_pkt(struct rte_mbuf* pkt) {
        /* FIXME: should we get a batch of buffered packets and then enqueue? Can we keep stats? */
        if(unlikely(rte_ring_enqueue(tx_ring, pkt) == -ENOBUFS)) {
                rte_pktmbuf_free(pkt);
                tx_stats->tx_drop[nf_info->client_id]++;
                return -ENOBUFS;
        }
        else tx_stats->tx_returned[nf_info->client_id]++;
        return 0;
}
