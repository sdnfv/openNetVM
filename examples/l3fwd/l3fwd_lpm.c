#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>

#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_lpm.h>
#include <rte_lpm6.h>
#include <rte_malloc.h>

#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"
#include "l3fwd.h"

/* Shared data structure containing host port info. */
extern struct port_info *ports;

struct ipv4_l3fwd_lpm_route {
        uint32_t ip; // destination address
        uint8_t  depth;
        uint8_t  if_out;
};

static struct ipv4_l3fwd_lpm_route ipv4_l3fwd_lpm_route_array[] = {
        {RTE_IPV4(1, 1, 1, 0), 24, 0},
        {RTE_IPV4(2, 1, 1, 0), 24, 1},
        {RTE_IPV4(3, 1, 1, 0), 24, 2},
        {RTE_IPV4(4, 1, 1, 0), 24, 3},
        {RTE_IPV4(5, 1, 1, 0), 24, 4},
        {RTE_IPV4(6, 1, 1, 0), 24, 5},
        {RTE_IPV4(7, 1, 1, 0), 24, 6},
        {RTE_IPV4(8, 1, 1, 0), 24, 7},
};

#define IPV4_L3FWD_LPM_NUM_ROUTES \
        (sizeof(ipv4_l3fwd_lpm_route_array) / sizeof(ipv4_l3fwd_lpm_route_array[0]))

#define IPV4_L3FWD_LPM_MAX_RULES         1024
#define IPV4_L3FWD_LPM_NUMBER_TBL8S (1 << 8)

int
setup_lpm(struct state_info *stats) {
        struct rte_lpm6_config config;
        int i, status, ret;
        char name[64];

        /* create the LPM table */
        stats->l3switch_req = (struct lpm_request *)rte_malloc(NULL, sizeof(struct lpm_request), 0);

        if (!stats->l3switch_req) return -1;

        snprintf(name, sizeof(name), "fw%d-%"PRIu64, rte_lcore_id(), rte_get_tsc_cycles());
        stats->l3switch_req->max_num_rules = IPV4_L3FWD_LPM_MAX_RULES;
        stats->l3switch_req->num_tbl8s = IPV4_L3FWD_LPM_NUMBER_TBL8S;
        stats->l3switch_req->socket_id = rte_socket_id();
        snprintf(stats->l3switch_req->name, sizeof(name), "%s", name);
        status = onvm_nflib_request_lpm(stats->l3switch_req);

        if (status < 0) {
                printf("Cannot get lpm region for l3switch\n");
                return -1;
        }
        rte_free(stats->l3switch_req);

        stats->lpm_tbl = rte_lpm_find_existing(name);

        if (stats->lpm_tbl == NULL) {
                printf("No existing LPM_TBL\n");
                return -1;
        }
        /* populate the LPM table */
        for (i = 0; i < IPV4_L3FWD_LPM_NUM_ROUTES; i++) {

                /* skip unused ports */
                if (get_initialized_ports(ipv4_l3fwd_lpm_route_array[i].if_out) == 0)
                        continue;

                ret = rte_lpm_add(stats->lpm_tbl,
                        ipv4_l3fwd_lpm_route_array[i].ip,
                        ipv4_l3fwd_lpm_route_array[i].depth,
                        ipv4_l3fwd_lpm_route_array[i].if_out);

                if (ret < 0) {
                        printf("Unable to add entry %u to the l3fwd LPM table. \n", i);
                        return -1;
                }

                printf("\nLPM: Adding route 0x%08x / %d (%d)\n",
                        (unsigned)ipv4_l3fwd_lpm_route_array[i].ip,
                        ipv4_l3fwd_lpm_route_array[i].depth,
                        ipv4_l3fwd_lpm_route_array[i].if_out);
                printf("IP: %" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8,
                        ipv4_l3fwd_lpm_route_array[i].ip >>24 & 0xFF, (ipv4_l3fwd_lpm_route_array[i].ip >> 16) & 0xFF,
                        (ipv4_l3fwd_lpm_route_array[i].ip >> 8) & 0xFF, (ipv4_l3fwd_lpm_route_array[i].ip) & 0xFF);
        }
        return 0;
}

uint16_t
lpm_get_ipv4_dst_port(void *ipv4_hdr, uint16_t portid, struct state_info *stats) {
        uint32_t next_hop;
        return (uint16_t) ((rte_lpm_lookup(stats->lpm_tbl,
                rte_be_to_cpu_32(((struct rte_ipv4_hdr *)ipv4_hdr)->dst_addr),
                &next_hop) == 0) ? next_hop : portid);
}

/*
 * This helper function checks if the destination port
 * is a valid port number that is currently binded to dpdk.
 */
int
get_initialized_ports(uint8_t if_out) {
        for (int i = 0; i < ports->num_ports; i++) {
                if (ports->id[i] == if_out)
                        return 1;
        }
        return 0;
}
