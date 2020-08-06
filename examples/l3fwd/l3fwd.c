/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2020 George Washington University
 *            2015-2020 University of California Riverside
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
 * l3switch.c - Layer 3 forwarding application with either exact match or LPM table.
 ********************************************************************/


#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_ip.h>
#include <rte_lpm.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>

#include "onvm_nflib.h"
#include "onvm_flow_table.h"
#include "onvm_pkt_helper.h"
#include "l3fwd.h"

#define NF_TAG "l3switch"

/* Print a usage message. */
static void
usage(const char *progname) {
        printf("Usage:\n");
        printf("%s [EAL args] -- [NF_LIB args] -- -p <print_delay>\n", progname);
        printf("%s -F <CONFIG_FILE.json> [EAL args] -- [NF_LIB args] -- [NF args]\n\n", progname);
        printf("Flags:\n");
        printf(" -e : Enable exact match. \n");
        printf(" -l : Enable longest prefix match. \n");
        printf(" -h : Specifies the hash entry number in decimal to be setup. Default is 4. \n");
}

/* Parse the application arguments. */
static int
parse_app_args(int argc, char *argv[], const char *progname, struct state_info *stats) {
        int c;

        while ((c = getopt(argc, argv, "h:p:e")) != -1) {
                switch (c) {
                        case 'h':
                                stats->hash_entry_number = strtoul(optarg, NULL, 10);
                                break;
                        case 'p':
                                stats->print_delay = strtoul(optarg, NULL, 10);
                                break;
                        case 'e':
                                stats->l3fwd_lpm_on = 0;
                                stats->l3fwd_em_on = 1;
                                break;
                        case '?':
                                usage(progname);
                                if (optopt == 'p')
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
        return optind;
}

/*
 * This function displays stats. It uses ANSI terminal codes to clear
 * screen when called. It is called from a single non-master
 * thread in the server process, when the process is run with more
 * than one lcore enabled.
 */
static void
print_stats(__attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx) {
        const char clr[] = {27, '[', '2', 'J', '\0'};
        const char topLeft[] = {27, '[', '1', ';', '1', 'H', '\0'};
        uint64_t total_packets = 0;

        struct onvm_nf *nf = nf_local_ctx->nf;
        struct state_info *stats = (struct state_info *)nf->data;

        /* Clear screen and move to top left */
        printf("\nPort statistics ====================================");
        int i;
        for (i = 0; i < ports->num_ports; i++) {
                printf("\nStatistics for port %u ------------------------------"
                          "\nPackets forwarded to: %20"PRIu64,
                           ports->id[i],
                           stats->port_statistics[ports->id[i]]);

               total_packets += stats->port_statistics[ports->id[i]];
        }
        printf("\nAggregate statistics ==============================="
                   "\nTotal packets forwarded: %17"PRIu64
                   "\nPackets dropped: %18"PRIu64,
                   total_packets,
                   stats->packets_dropped);
        printf("\n====================================================\n");

        printf("\n\n");
}

/*
 * This function checks for valid ipv4 packets. Updates the
 * src and destination ethernet addresses of packets. It then performs a lookup
 * for the destination port. If the destination port value returned is not valid/not binded to dpdk,
 * the packet is forwarded back to the port of incoming traffic.
 */
static int
packet_handler(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta,
               __attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx) {
        static uint32_t counter = 0;

        struct onvm_nf *nf = nf_local_ctx->nf;
        struct state_info *stats = (struct state_info *)nf->data;

        if (counter++ == stats->print_delay) {
               print_stats(nf_local_ctx);
               counter = 0;
        }
        struct rte_ether_hdr *eth_hdr;
        struct ipv4_hdr *ipv4_hdr;
        uint16_t dst_port;

        eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
        if (onvm_pkt_is_ipv4(pkt)) {
                /* Handle IPv4 headers.*/
                ipv4_hdr = rte_pktmbuf_mtod_offset(pkt, struct ipv4_hdr *,
                                                   sizeof(struct rte_ether_hdr));

#ifdef DO_RFC_1812_CHECKS
                /* Check to make sure the packet is valid (RFC1812) */
                if (is_valid_ipv4_pkt(ipv4_hdr, pkt->pkt_len) < 0) {
                        meta->action = ONVM_NF_ACTION_DROP;
                        packets_dropped++;
                        return 0;
                }
#endif
                if (stats->l3fwd_lpm_on) {
                        dst_port = lpm_get_ipv4_dst_port(ipv4_hdr, pkt->port, stats);
                } else {
                        dst_port = em_get_ipv4_dst_port(pkt, stats);
                }
                if (dst_port >= RTE_MAX_ETHPORTS ||
                        get_initialized_ports(dst_port) == 0)
                        dst_port = pkt->port;

#ifdef DO_RFC_1812_CHECKS
                /* Update time to live and header checksum */
                --(ipv4_hdr->time_to_live);
                ++(ipv4_hdr->hdr_checksum);
#endif
                /* dst addr */
                *(uint64_t *)&eth_hdr->d_addr = stats->dest_eth_addr[dst_port];

                /* src addr */
                rte_ether_addr_copy(&stats->ports_eth_addr[dst_port], &eth_hdr->s_addr);

                meta->destination = dst_port;
                stats->port_statistics[dst_port]++;
                meta->action = ONVM_NF_ACTION_OUT;
        } else {
                meta->action = ONVM_NF_ACTION_DROP;
                stats->packets_dropped++;
        }
        return 0;
}

/*
 * This function displays the ethernet addressof each initialized port.
 * It saves the ethernet addresses in the struct ether_addr array.
 */
static void
l3fwd_initialize_ports(struct state_info *stats) {
        uint16_t i;
        for (i = 0; i < ports->num_ports; i++) {
                rte_eth_macaddr_get(ports->id[i], &stats->ports_eth_addr[ports->id[i]]);
                printf("Port %u, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
                        ports->id[i],
                        stats->ports_eth_addr[ports->id[i]].addr_bytes[0],
                        stats->ports_eth_addr[ports->id[i]].addr_bytes[1],
                        stats->ports_eth_addr[ports->id[i]].addr_bytes[2],
                        stats->ports_eth_addr[ports->id[i]].addr_bytes[3],
                        stats->ports_eth_addr[ports->id[i]].addr_bytes[4],
                        stats->ports_eth_addr[ports->id[i]].addr_bytes[5]);
        }
}

/* 
 * This function pre-init dst MACs for all ports to 02:00:00:00:00:xx.
 * Destination mac addresses are saved in th dest_eth_addr array.
 */
static void
l3fwd_initialize_dst(struct state_info *stats) {
        uint16_t i;
        for (i = 0; i < ports->num_ports; i++) {
                stats->dest_eth_addr[ports->id[i]] =
                        RTE_ETHER_LOCAL_ADMIN_ADDR + ((uint64_t)ports->id[i] << 40);
                *(uint64_t *)(stats->val_eth + ports->id[i]) = stats->dest_eth_addr[ports->id[i]];
        }
}

/* This function frees all allocated data structures and hash tables. */
static void
free_tables(struct state_info *stats) {
        if (stats->lpm_tbl != NULL) {
                rte_lpm_free(stats->lpm_tbl);
        }
        if (stats->em_tbl != NULL) {
                onvm_ft_free(stats->em_tbl);
        }
}

void
nf_setup(struct onvm_nf_local_ctx *nf_local_ctx) {
        struct onvm_nf *nf = nf_local_ctx->nf;
        struct state_info *stats = (struct state_info *)nf->data;
        l3fwd_initialize_ports(stats);
        l3fwd_initialize_dst(stats);
        /*
         * Hash flags are valid only for exact macth,
         * reset them to default for longest-prefix match.
         */
        if (stats->l3fwd_lpm_on) {
                if (setup_lpm(stats) < 0) {
                     onvm_nflib_stop(nf_local_ctx);
                     rte_free(stats);
                     rte_exit(EXIT_FAILURE, "Unable to setup LPM\n");
                }
                stats->hash_entry_number = HASH_ENTRY_NUMBER_DEFAULT;
                printf("\nLongest prefix match enabled. \n");
        } else {
                if (setup_hash(stats) < 0) {
                     onvm_nflib_stop(nf_local_ctx);
                     rte_free(stats);
                     rte_exit(EXIT_FAILURE, "Unable to setup Hash\n");
                }
                printf("Hash table exact match enabled. \n");
                printf("Hash entry number set to: %d\n", stats->hash_entry_number);
        }
}

int
main(int argc, char *argv[]) {
        int arg_offset;
        struct onvm_nf_local_ctx *nf_local_ctx;
        struct onvm_nf_function_table *nf_function_table;
        const char *progname = argv[0];

        nf_local_ctx = onvm_nflib_init_nf_local_ctx();
        onvm_nflib_start_signal_handler(nf_local_ctx, NULL);

        nf_function_table = onvm_nflib_init_nf_function_table();
        nf_function_table->pkt_handler = &packet_handler;
        nf_function_table->setup = &nf_setup;

        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG, nf_local_ctx, nf_function_table)) < 0) {
                onvm_nflib_stop(nf_local_ctx);
                if (arg_offset == ONVM_SIGNAL_TERMINATION) {
                        printf("Exiting due to user termination\n");
                        return 0;
                } else {
                        rte_exit(EXIT_FAILURE, "Failed ONVM init\n");
                }
        }

        argc -= arg_offset;
        argv += arg_offset;

        /*
         * The following allocates a struct which keeps track of all NF state information.
         * Longest prefix match is enabled by default as well as default values for print delay
         * and hash entry number.
         */
        struct onvm_nf *nf = nf_local_ctx->nf;
        struct state_info *stats = rte_calloc("state", 1, sizeof(struct state_info), 0); // Will be freed my manager.
        if (stats == NULL) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Unable to initialize NF stats.");
        }
        stats->print_delay = 1000000;
        stats->l3fwd_lpm_on = 1;
        stats->l3fwd_em_on = 0;
        stats->hash_entry_number = HASH_ENTRY_NUMBER_DEFAULT;
        nf->data = (void *)stats;

        /* Parse application arguments. */
        if (parse_app_args(argc, argv, progname, stats) < 0) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        }

        if (ports->num_ports == 0) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "No Ethernet ports. Ensure ports binded to dpdk. - bye\n");
        }
        onvm_nflib_run(nf_local_ctx);

        free_tables(stats);
        onvm_nflib_stop(nf_local_ctx);
        printf("If we reach here, program is ending\n");
        return 0;
}
