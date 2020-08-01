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
 * l2switch.c - send all packets from one port out the adjacent port.
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
#include <rte_mbuf.h>
#include <rte_malloc.h>

#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"

#define NF_TAG "l2switch"

/* Shared data structure containing host port info. */
extern struct port_info *ports;

/* Per-port statistics struct. */
struct l2fwd_port_statistics {
	uint64_t tx;
	uint64_t rx;
	uint64_t dropped;
} __rte_cache_aligned;

/*Struct that holds all NF state information */
struct state_info {
       /* Number of package between each print. */
       uint32_t print_delay;
       /* List of enabled ports. */
       uint32_t l2fwd_dst_ports[RTE_MAX_ETHPORTS];
       /* Ethernet addresses of ports. */
       struct rte_ether_addr l2fwd_ports_eth_addr[RTE_MAX_ETHPORTS];
       struct l2fwd_port_statistics port_statistics[RTE_MAX_ETHPORTS];
       /* MAC updating enabled by default */
       int mac_updating;
       /* Print mac address disabled by default */
       int print_mac;
};

/*
 * Print a usage message
 */
static void
usage(const char *progname) {
        printf("Usage:\n");
        printf("%s [EAL args] -- [NF_LIB args] -- -p <print_delay>\n", progname);
        printf("%s -F <CONFIG_FILE.json> [EAL args] -- [NF_LIB args] -- [NF args]\n\n", progname);
        printf("Flags:\n");
        printf(" - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.\n");
        printf(" - `-n : Disables mac updating. \n");
        printf(" - `-m : Enables printing updated mac address. \n");
}

/*
 * Parse the application arguments.
 */
static int
parse_app_args(int argc, char *argv[], const char *progname, struct state_info *stats) {
        int c;

        while ((c = getopt(argc, argv, "p:nm")) != -1) {
                switch (c) {
                        case 'p':
                                stats->print_delay = strtoul(optarg, NULL, 10);
                                break;
                        case 'n':
                                /* Disable MAC updating. */
                                stats->mac_updating = 0;
                                break;
                        case 'm':
                                /* Enable printing of MAC address.*/
                                stats->print_mac = 1;
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
print_stats(struct state_info *stats) {
	uint64_t total_packets_dropped, total_packets_tx, total_packets_rx;
	unsigned i;

	total_packets_dropped = 0;
	total_packets_tx = 0;
	total_packets_rx = 0;

	const char clr[] = { 27, '[', '2', 'J', '\0' };
	const char topLeft[] = { 27, '[', '1', ';', '1', 'H','\0' };

		/* Clear screen and move to top left */
	printf("%s%s", clr, topLeft);

	printf("\nPort statistics ====================================");

	for (i = 0; i < ports->num_ports; i++) {
		printf("\nStatistics for port %u ------------------------------"
			   "\nPackets sent: %24"PRIu64
			   "\nPackets received: %20"PRIu64
			   "\nForwarding to port: %u",
			   ports->id[i],
			   stats->port_statistics[ports->id[i]].tx,
			   stats->port_statistics[ports->id[i]].rx,
			   stats->l2fwd_dst_ports[ports->id[i]]);

		total_packets_tx += stats->port_statistics[ports->id[i]].tx;
		total_packets_rx += stats->port_statistics[ports->id[i]].rx;
	}
	printf("\nAggregate statistics ==============================="
		   "\nTotal packets sent: %18"PRIu64
		   "\nTotal packets received: %14"PRIu64,
		   total_packets_tx,
		   total_packets_rx);
	printf("\n====================================================\n");
}
/*
 * This function displays the ethernet addressof each initialized port.
 * It saves the ethernet addresses in the struct ether_addr array.
 */
static void
l2fwd_initialize_ports(struct state_info *stats) {
        uint16_t i;
        for (i = 0; i < ports->num_ports; i++) {
                rte_eth_macaddr_get(ports->id[i], &stats->l2fwd_ports_eth_addr[ports->id[i]]);
                printf("Port %u, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
                        ports->id[i],
                        stats->l2fwd_ports_eth_addr[ports->id[i]].addr_bytes[0],
                        stats->l2fwd_ports_eth_addr[ports->id[i]].addr_bytes[1],
                        stats->l2fwd_ports_eth_addr[ports->id[i]].addr_bytes[2],
                        stats->l2fwd_ports_eth_addr[ports->id[i]].addr_bytes[3],
                        stats->l2fwd_ports_eth_addr[ports->id[i]].addr_bytes[4],
                        stats->l2fwd_ports_eth_addr[ports->id[i]].addr_bytes[5]);
        }
}

/* The source MAC address is replaced by the TX_PORT MAC address */
/* The destination MAC address is replaced by 02:00:00:00:00:TX_PORT_ID */
static void
l2fwd_mac_updating(struct rte_mbuf *pkt, unsigned dest_portid, struct state_info *stats) {
        struct rte_ether_hdr *eth;
        void *tmp;
        eth = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);

        /* 02:00:00:00:00:xx */
        tmp = &eth->d_addr.addr_bytes[0];
        *((uint64_t *)tmp) = 0x000000000002 + ((uint64_t)dest_portid << 40);
        rte_ether_addr_copy(tmp, &eth->s_addr);

        if (stats->print_mac) {
                printf("Packet updated MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
                        eth->s_addr.addr_bytes[0],
                        eth->s_addr.addr_bytes[1],
                        eth->s_addr.addr_bytes[2],
                        eth->s_addr.addr_bytes[3],
                        eth->s_addr.addr_bytes[4],
                        eth->s_addr.addr_bytes[5]);
        }
}

/* The destination port is the adjacent port from the enabled portmask, that is,
 * if the first four ports are enabled (portmask 0xf),
 * ports 1 and 2 forward into each other, and ports 3 and 4 forward into each other.
*/
static void
l2fwd_set_dest_ports(struct state_info *stats) {
        int i;
        unsigned nb_ports_in_mask = 0;
        int last_port = 0;
        for (i = 0; i < ports->num_ports; i++) {
                if (nb_ports_in_mask % 2) {
                        stats->l2fwd_dst_ports[ports->id[i]] = last_port;
                        stats->l2fwd_dst_ports[last_port] = ports->id[i];
                } else {
                        last_port = ports->id[i];
                }
                nb_ports_in_mask++;
        }
        if (nb_ports_in_mask % 2) {
                        printf("Notice: odd number of ports in portmask.\n");
                        stats->l2fwd_dst_ports[last_port] = last_port;
        }

}

static int
packet_handler(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta,
               __attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx) {
        static uint32_t counter = 0;
        struct onvm_nf *nf = nf_local_ctx->nf;
        struct state_info *stats = (struct state_info *)nf->data;
        if (++counter == stats->print_delay) {
                print_stats(stats);
                counter = 0;
        }
        if (pkt->port > RTE_MAX_ETHPORTS) {
                RTE_LOG(INFO, APP, "Packet source port greater than MAX ethernet ports allowed. \n");
                meta->action = ONVM_NF_ACTION_DROP;
                return 0;
        }
        /* Update stats packet received on port. */
        stats->port_statistics[pkt->port].rx += 1;
        unsigned dst_port = stats->l2fwd_dst_ports[pkt->port];

        /* If mac_updating enabled update source and destination mac address of packet. */
        if (stats->mac_updating)
                l2fwd_mac_updating(pkt, dst_port, stats);

        /* Set destination port of packet. */
        meta->destination = dst_port;
        /* Update stats packet sent from source port. */
        stats->port_statistics[dst_port].tx += 1;
        meta->action = ONVM_NF_ACTION_OUT;
        return 0;
}

void
nf_setup(struct onvm_nf_local_ctx *nf_local_ctx) {
        struct onvm_nf *nf = nf_local_ctx->nf;
        struct state_info *stats = (struct state_info *)nf->data;

        /* Initialize port stats. */
        memset(&stats->port_statistics, 0, sizeof(stats->port_statistics));

        /* Set destination port for each port. */
        l2fwd_set_dest_ports(stats);

        /* Get mac address for each port.  */
        l2fwd_initialize_ports(stats);

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
        if (ports->num_ports == 0) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "No Ethernet ports. Ensure ports binded to dpdk. - bye\n");
        }
        argc -= arg_offset;
        argv += arg_offset;

        struct onvm_nf *nf = nf_local_ctx->nf;
        struct state_info *stats = rte_calloc("state", 1, sizeof(struct state_info), 0);
        if (stats == NULL) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Unable to initialize NF stats.");
        }
        /* MAC updating enabled by default */
        stats->mac_updating = 1;
        /* Print mac address disabled by default */
        stats->print_mac = 0;
        stats->print_delay = 1000000;
        nf->data = (void *)stats;

        if (parse_app_args(argc, argv, progname, stats) < 0) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        }
        RTE_LOG(INFO, APP, "MAC updating %s\n", stats->mac_updating ? "enabled" : "disabled");

        onvm_nflib_run(nf_local_ctx);

        onvm_nflib_stop(nf_local_ctx);
        printf("If we reach here, program is ending\n");
        return 0;
}
