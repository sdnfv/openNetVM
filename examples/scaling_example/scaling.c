/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2017 George Washington University
 *            2015-2017 University of California Riverside
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
 *     * The name of the author may not be used to endorse or promote
 *       products derived from this software without specific prior
 *       written permission.
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
 * scaling.c - Example usage of the provided scaling functionality.
 ********************************************************************/

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_mempool.h>
#include <rte_cycles.h>
#include <rte_ring.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_malloc.h>

#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"
#include "onvm_flow_table.h"

#define NF_TAG "scaling"

#define PKTMBUF_POOL_NAME "MProc_pktmbuf_pool"
#define PKT_READ_SIZE  ((uint16_t)32)
#define SPEED_TESTER_BIT 7
#define LATENCY_BIT 6
#define LOCAL_EXPERIMENTAL_ETHER 0x88B5
#define DEFAULT_PKT_NUM 128
#define MAX_PKT_NUM NF_QUEUE_RINGSIZE

/* number of package between each print */
static uint32_t print_delay = 10000000;
static uint16_t destination;
static uint8_t use_direct_rings = 0;
static uint8_t keep_running = 1;

static uint16_t packet_size = ETHER_HDR_LEN;
static uint8_t d_addr_bytes[ETHER_ADDR_LEN];

/* Default number of packets: 128; user can modify it by -c <packet_number> in command line */
static uint32_t packet_number = 0;

void nf_setup(struct onvm_nf_info *nf_info);

/*
 * Print a usage message
 */
static void
usage(const char *progname) {
        printf("Usage: %s [EAL args] -- [NF_LIB args] -- -d <destination> [-p <print_delay>] "
                        "[-a] [-s <packet_length>] [-m <dest_mac_address>] [-o <pcap_filename>] "
                        "[-c <packet_number>] [-l]\n\n", progname);
}

/*
 * Parse the application arguments.
 */
static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c, dst_flag = 0;

        while ((c = getopt(argc, argv, "d:p:a")) != -1) {
                switch (c) {
                case 'a':
                        use_direct_rings = 1;
                        break;
                case 'd':
                        destination = strtoul(optarg, NULL, 10);
                        dst_flag = 1;
                        break;
                case 'p':
                        print_delay = strtoul(optarg, NULL, 10);
                        break;
               case '?':
                        usage(progname);
                        if (optopt == 'd')
                                RTE_LOG(INFO, APP, "Option -%c requires an argument.\n", optopt);
                        else if (optopt == 'p')
                                RTE_LOG(INFO, APP, "Option -%c requires an argument.\n", optopt);
                        else if (isprint(optopt))
                                RTE_LOG(INFO, APP, "Unknown option `-%c'.\n", optopt);
                        else
                                RTE_LOG(INFO, APP, "Unknown option character `\\x%x'.\n", optopt);
                        return -1;
                default:
                        usage(progname);
                        return -1;
                }
        }

        if (!dst_flag) {
                RTE_LOG(INFO, APP, "Speed tester NF requires a destination NF with the -d flag.\n");
                return -1;
        }

        return optind;
}

/*
 * This function displays stats. It uses ANSI terminal codes to clear
 * screen when called. It is called from a single non-master
 * thread in the server process, when the process is run with more
 * than one lcore enabled.
 */
static void
do_stats_display(struct rte_mbuf* pkt) {
        static uint64_t last_cycles;
        static uint64_t cur_pkts = 0;
        static uint64_t last_pkts = 0;
        const char clr[] = { 27, '[', '2', 'J', '\0' };
        const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };
        (void)pkt;

        uint64_t cur_cycles = rte_get_tsc_cycles();
        cur_pkts += print_delay;

        /* Clear screen and move to top left */
        printf("%s%s", clr, topLeft);

        printf("Total packets: %9"PRIu64" \n", cur_pkts);
        printf("TX pkts per second: %9"PRIu64" \n", (cur_pkts - last_pkts)
                * rte_get_timer_hz() / (cur_cycles - last_cycles));
        printf("Initial packets created: %u\n", packet_number);

        last_pkts = cur_pkts;
        last_cycles = cur_cycles;

        printf("\n\n");
}

static int
packet_handler_child(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta, __attribute__((unused)) struct onvm_nf_info *nf_info) {
        (void)pkt;
        meta->destination = *(uint16_t *)nf_info->data;
        meta->action = ONVM_NF_ACTION_TONF;

        return 0;
}

static int
packet_handler(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta, __attribute__((unused)) struct onvm_nf_info *nf_info) {
        static uint32_t counter = 0;
        static uint32_t spawned = 0;
        static int ret = 0;
        if (counter++ == print_delay) {
                do_stats_display(pkt);
                counter = 0;
        }

        /* Testing NF scaling */ 
        if (counter == 12345 && spawned == 0){
                struct onvm_nf_scale_info *scale_info;
                uint16_t send_to = nf_info->service_id;
                uint16_t *q;
                while (ret == 0) {
                        scale_info = rte_malloc("nf_scale_info", sizeof(struct onvm_nf_scale_info), 0);
                        q = rte_malloc("nf_state_data", sizeof(uint16_t), 0);
                        
                        /* Sets service id of child */
                        *q = send_to;
                        scale_info->service_id = ++send_to;
                        /* Let manager handle instance id assignment */
                        scale_info->instance_id = -1;
                        /* Run the setup function to generate packets */
                        scale_info->setup_func = &nf_setup;
                        /* Custom packet handler */
                        scale_info->pkt_func = &packet_handler_child;
                        /* Insert state data, will be used to forward packets to itself */
                        scale_info->data = q;
                        scale_info->parent = nf_info;

                        /* Spawn the child */
                        ret = onvm_nflib_scale(scale_info);
                }
                *(uint16_t *)nf_info->data = *q;
                spawned = 1;
        }

        if (ONVM_CHECK_BIT(meta->flags, SPEED_TESTER_BIT)) {
                /* one of our fake pkts to forward */
                meta->destination = *(uint16_t *)nf_info->data;
                meta->action = ONVM_NF_ACTION_TONF;
        } else {
                /* Drop real incoming packets */
                meta->action = ONVM_NF_ACTION_DROP;
        }
        
        return 0;
}


static void
handle_signal(int sig) {
        if (sig == SIGINT || sig == SIGTERM)
                keep_running = 0;
}


static void
run_advanced_rings(struct onvm_nf_info *nf_info) {
        void *pkts[PKT_READ_SIZE];
        struct onvm_pkt_meta* meta;
        uint16_t i, j, nb_pkts;
        void *pktsTX[PKT_READ_SIZE];
        int tx_batch_size;
        struct rte_ring *rx_ring;
        struct rte_ring *tx_ring;
        volatile struct onvm_nf *nf;

        printf("Process %d handling packets using advanced rings\n", nf_info->instance_id);
        printf("[Press Ctrl-C to quit ...]\n");

        /* Listen for ^C and docker stop so we can exit gracefully */
        signal(SIGINT, handle_signal);
        signal(SIGTERM, handle_signal);

        /* Get rings from nflib */
        nf = onvm_nflib_get_nf(nf_info->instance_id);
        rx_ring = nf->rx_q;
        tx_ring = nf->tx_q;

        /* Testing NF scaling */ 
        nf->nf_advanced_rings_function = &run_advanced_rings;
        static uint8_t spawned = 0;
        if (spawned == 0){
                uint16_t *q = rte_malloc("q", sizeof(uint16_t),0);
                *q = (uint16_t) (1 + rand() % 15);
                struct onvm_nf_scale_info *scale_info = rte_malloc("hehehe", sizeof(struct onvm_nf_scale_info), 0);
                scale_info->parent = nf_info;
                scale_info->data = q;
                scale_info->instance_id = -1;
                scale_info->service_id = *q;
                scale_info->setup_func = &nf_setup;
                scale_info->adv_rings_func = &run_advanced_rings;
                while(onvm_nflib_scale(scale_info)==0);
                spawned=1;
        }


        while (keep_running && rx_ring && tx_ring && nf) {
                tx_batch_size = 0;
                /* Dequeue all packets in ring up to max possible. */
                nb_pkts = rte_ring_dequeue_burst(rx_ring, pkts, PKT_READ_SIZE, NULL);

                if (unlikely(nb_pkts == 0)) {
                        continue;
                }
                /* Process all the packets */
                for (i = 0; i < nb_pkts; i++) {
                        meta = onvm_get_pkt_meta((struct rte_mbuf*)pkts[i]);
                        packet_handler((struct rte_mbuf*)pkts[i], meta, nf_info);
                        pktsTX[tx_batch_size++] = pkts[i];
                }

                if (unlikely(tx_batch_size > 0 && rte_ring_enqueue_bulk(tx_ring, pktsTX, tx_batch_size, NULL) == 0)) {
                        nf->stats.tx_drop += tx_batch_size;
                        for (j = 0; j < tx_batch_size; j++) {
                                rte_pktmbuf_free(pktsTX[j]);
                        }
                } else {
                        nf->stats.tx += tx_batch_size;
                }
        }
        onvm_nflib_stop(nf_info);
}


/*
 * Generates fake packets or loads them from a pcap file
 */
void
nf_setup(struct onvm_nf_info *nf_info) {
        uint32_t i;
        struct rte_mempool *pktmbuf_pool;

        pktmbuf_pool = rte_mempool_lookup(PKTMBUF_POOL_NAME);
        if (pktmbuf_pool == NULL) {
                onvm_nflib_stop(nf_info);
                rte_exit(EXIT_FAILURE, "Cannot find mbuf pool!\n");
        }
        packet_number = (packet_number? packet_number : DEFAULT_PKT_NUM);

        printf("Creating %u packets to send to %u\n", packet_number, destination);

        for (i = 0; i < packet_number; ++i) {
                struct onvm_pkt_meta* pmeta;
                struct ether_hdr *ehdr;
                int j;

                struct rte_mbuf *pkt = rte_pktmbuf_alloc(pktmbuf_pool);

                /* set up ether header and set new packet size */
                ehdr = (struct ether_hdr *) rte_pktmbuf_append(pkt, packet_size);

                /* Using manager mac addr for source*/
                rte_eth_macaddr_get(0, &ehdr->s_addr);
                for (j = 0; j < ETHER_ADDR_LEN; ++j) {
                        ehdr->d_addr.addr_bytes[j] = d_addr_bytes[j];
                }
                ehdr->ether_type = LOCAL_EXPERIMENTAL_ETHER;

                pmeta = onvm_get_pkt_meta(pkt);
                pmeta->destination = *(uint16_t * )nf_info->data;
                pmeta->action = ONVM_NF_ACTION_TONF;
                pmeta->flags = ONVM_SET_BIT(0, SPEED_TESTER_BIT);
                pkt->hash.rss = i;
                pkt->port = 0;

                onvm_nflib_return_pkt(nf_info, pkt);
        }
}


int main(int argc, char *argv[]) {
        int arg_offset;
        const char *progname = argv[0];
        struct onvm_nf_info *nf_info;

        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG, &nf_info)) < 0)
                return -1;

        argc -= arg_offset;
        argv += arg_offset;

        if (parse_app_args(argc, argv, progname) < 0) {
                onvm_nflib_stop(nf_info);
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        }

        /* Set the function to execute before running the NF
         * For advanced rings manually run the function */
        onvm_nflib_set_setup_function(nf_info, &nf_setup);

        srand(time(NULL));
        nf_info->data = (void *)rte_malloc("nf_specific_data", sizeof(int), 0);
        *(uint16_t*)nf_info->data = nf_info->service_id;

        if (use_direct_rings) {
                nf_setup(nf_info);
                onvm_nflib_nf_ready(nf_info);
                run_advanced_rings(nf_info);
        } else {
                onvm_nflib_run(nf_info, &packet_handler);
        }
        printf("If we reach here, program is ending\n");
        return 0;
}
