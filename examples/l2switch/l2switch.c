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

#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"

#define NF_TAG "l2switch"

/* Number of package between each print. */
static uint32_t print_delay = 1000000;

/* List of enabled ports. */
static uint32_t l2fwd_dst_ports[RTE_MAX_ETHPORTS];

/* Ethernet addresses of ports. */
static struct ether_addr l2fwd_ports_eth_addr[RTE_MAX_ETHPORTS];

/* Per-port statistics struct. */
struct l2fwd_port_statistics {
	uint64_t tx;
	uint64_t rx;
	uint64_t dropped;
} __rte_cache_aligned;
struct l2fwd_port_statistics port_statistics[RTE_MAX_ETHPORTS];

/* Shared data structure containing host port info. */
extern struct port_info *ports;

/* MAC updating enabled by default */
static int mac_updating = 0;

/* Print mac address disabled by default */
static int print_mac = 1;

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
}

/*
 * Parse the application arguments.
 */
static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c;

        while ((c = getopt(argc, argv, "p:n:")) != -1) {
                switch (c) {
                        case 'p':
                                print_delay = strtoul(optarg, NULL, 10);
                                break;
                        case 'n':
                                /* Disable MAC updating. */
                                mac_updating = 1;
                                break;
                        case 'm':
                                /* Enable printing of MAC address.*/
                                print_mac = 0;
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
print_stats(void)
{
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

	for (i = 0; i < ports -> num_ports; i++) {
		printf("\nStatistics for port %u ------------------------------"
			   "\nPackets sent: %24"PRIu64
			   "\nPackets received: %20"PRIu64
			   "\nForwarding to port: %u"PRIu64,
			   ports->id[ports -> id[i]],
			   port_statistics[ports -> id[i]].tx,
			   port_statistics[ports -> id[i]].rx,
			   l2fwd_dst_ports[ports -> id[i]]);

		total_packets_tx += port_statistics[ports -> id[i]].tx;
		total_packets_rx += port_statistics[ports -> id[i]].rx;
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
l2fwd_initialize_ports(void) {
        uint16_t i;
        for (i = 0; i < ports -> num_ports; i++) {
                rte_eth_macaddr_get(ports -> id[i], &l2fwd_ports_eth_addr[ports -> id[i]]);
		printf("Port %u, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
                        ports ->id[i],
                        l2fwd_ports_eth_addr[ports -> id[i]].addr_bytes[0],
                        l2fwd_ports_eth_addr[ports -> id[i]].addr_bytes[1],
                        l2fwd_ports_eth_addr[ports -> id[i]].addr_bytes[2],
                        l2fwd_ports_eth_addr[ports -> id[i]].addr_bytes[3],
                        l2fwd_ports_eth_addr[ports -> id[i]].addr_bytes[4],
                        l2fwd_ports_eth_addr[ports -> id[i]].addr_bytes[5]);
        }
}

/* The source MAC address is replaced by the TX_PORT MAC address */
/* The destination MAC address is replaced by 02:00:00:00:00:TX_PORT_ID */
static void
l2fwd_mac_updating(struct rte_mbuf *pkt, unsigned dest_portid)
{
	struct ether_hdr *eth;
	void *tmp;

	eth = rte_pktmbuf_mtod(pkt, struct ether_hdr *);

	/* 02:00:00:00:00:xx */
	tmp = &eth->d_addr.addr_bytes[0];
	*((uint64_t *)tmp) = 0x000000000002 + ((uint64_t)dest_portid << 40);

	ether_addr_copy(&l2fwd_ports_eth_addr[dest_portid], &eth->s_addr);

        if (print_mac) {
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
l2fwd_set_dest_ports(){
        int i;
        unsigned nb_ports_in_mask = 0;
        int l2fwd_dst_ports[RTE_MAX_ETHPORTS];
        int last_port = 0;
        for (i = 0; i < ports -> num_ports; i++) {
                if (nb_ports_in_mask % 2){ 
                        l2fwd_dst_ports[ports -> id[i]] = last_port;
                        l2fwd_dst_ports[last_port] = ports -> id[i];
                        nb_ports_in_mask++;
                }
                else{
                        last_port = ports -> id[i];
                        nb_ports_in_mask++;
                } 
        }
        if (nb_ports_in_mask % 2) {
                        printf("Notice: odd number of ports in portmask.\n");
                        l2fwd_dst_ports[last_port] = last_port;
        }
}
static int
packet_handler(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta,
               __attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx) {
        static uint32_t counter = 0;
        if (counter++ == print_delay) {
                print_stats();
                counter = 0;
        }
        /* Update stats packet received on port. */
        port_statistics[pkt -> port].rx += 1;
        unsigned dst_port = l2fwd_dst_ports[pkt->port];

        /* If mac_updating enabled update source and destination mac address of packet. */
	if (mac_updating)
		l2fwd_mac_updating(pkt, dst_port);

        meta->destination = dst_port;

        /* Update stats packet sent from source port. */
        port_statistics[dst_port].tx += 1;

        meta->action = ONVM_NF_ACTION_OUT;
        return 0;
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

        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG, nf_local_ctx, nf_function_table)) < 0) {
                onvm_nflib_stop(nf_local_ctx);
                if (arg_offset == ONVM_SIGNAL_TERMINATION) {
                        printf("Exiting due to user termination\n");
                        return 0;
                } else {
                        rte_exit(EXIT_FAILURE, "Failed ONVM init\n");
                }
        }
        if (ports -> num_ports == 0) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "No Ethernet ports. Ensure ports binded to dpdk. - bye\n");
        }
        argc -= arg_offset;
        argv += arg_offset;

        /* Initialize port stats. */
        memset(&port_statistics, 0, sizeof(port_statistics));
        
        /* Set destination port for each port. */
        l2fwd_set_dest_ports();

        /* Get mac address for each port.  */
        l2fwd_initialize_ports();

        if (parse_app_args(argc, argv, progname) < 0) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        }
        RTE_LOG(INFO, APP, "MAC updating %s\n", mac_updating ? "enabled" : "disabled");

        onvm_nflib_run(nf_local_ctx);

        onvm_nflib_stop(nf_local_ctx);
        printf("If we reach here, program is ending\n");
        return 0;
}
