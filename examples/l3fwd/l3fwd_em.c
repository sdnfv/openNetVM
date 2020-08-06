#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_hash.h>
#include <rte_ip.h>
#include <rte_mbuf.h>

#include "l3fwd.h"
#include "onvm_flow_table.h"
#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"

#define BYTE_VALUE_MAX 256

struct ipv4_l3fwd_em_route {
    struct onvm_ft_ipv4_5tuple key;
    uint8_t if_out;
};

// src_addr, dst_addr, src_port, dst_port, proto
struct ipv4_l3fwd_em_route ipv4_l3fwd_em_route_array[] = {
    {{RTE_IPV4(101, 0, 0, 0), RTE_IPV4(100, 10, 0, 1),  0, 1, IPPROTO_TCP}, 0},
    {{RTE_IPV4(201, 0, 0, 0), RTE_IPV4(200, 20, 0, 1),  1, 0, IPPROTO_TCP}, 1},
    {{RTE_IPV4(111, 0, 0, 0), RTE_IPV4(100, 30, 0, 1),  0, 2, IPPROTO_TCP}, 2},
    {{RTE_IPV4(211, 0, 0, 0), RTE_IPV4(200, 40, 0, 1),  2, 0, IPPROTO_TCP}, 3},
};

/* Struct that holds info about each flow, and is stored at each flow table entry. */
struct data {
    uint8_t  if_out;
};

#define IPV4_L3FWD_EM_NUM_ROUTES \
    (sizeof(ipv4_l3fwd_em_route_array) / sizeof(ipv4_l3fwd_em_route_array[0]))

static inline void
populate_ipv4_few_flow_into_table(struct onvm_ft *h) {
    uint32_t i;
    int32_t ret;

    for (i = 0; i < IPV4_L3FWD_EM_NUM_ROUTES; i++) {
        struct ipv4_l3fwd_em_route entry;
        union ipv4_5tuple_host newkey;
        struct data *data = NULL;

        entry = ipv4_l3fwd_em_route_array[i];
        int tbl_index = onvm_ft_add_key(h, &entry.key, (char **)&data);
        data->if_out = entry.if_out;
        if (tbl_index < 0)
            rte_exit(EXIT_FAILURE, "Unable to add entry %u\n", i);
        printf("\nAdding key:");
        _onvm_ft_print_key(&entry.key);
    }
    printf("Hash: Adding 0x%" PRIx64 " keys\n",
        (uint64_t)IPV4_L3FWD_EM_NUM_ROUTES);
}

#define NUMBER_PORT_USED 4
static inline void
populate_ipv4_many_flow_into_table(struct onvm_ft *h, unsigned int nr_flow) {
    unsigned i;

    for (i = 0; i < nr_flow; i++) {
        struct ipv4_l3fwd_em_route entry;
        union ipv4_5tuple_host newkey;

        uint8_t a = (uint8_t)
            ((i/NUMBER_PORT_USED)%BYTE_VALUE_MAX);
        uint8_t b = (uint8_t)
            (((i/NUMBER_PORT_USED)/BYTE_VALUE_MAX)%BYTE_VALUE_MAX);
        uint8_t c = (uint8_t)
            ((i/NUMBER_PORT_USED)/(BYTE_VALUE_MAX*BYTE_VALUE_MAX));

        /* Create the ipv4 exact match flow */
        memset(&entry, 0, sizeof(entry));
        switch (i & (NUMBER_PORT_USED - 1)) {
        case 0:
            entry = ipv4_l3fwd_em_route_array[0];
            entry.key.dst_addr = RTE_IPV4(101, c, b, a);
            break;
        case 1:
            entry = ipv4_l3fwd_em_route_array[1];
            entry.key.dst_addr = RTE_IPV4(201, c, b, a);
            break;
        case 2:
            entry = ipv4_l3fwd_em_route_array[2];
            entry.key.dst_addr = RTE_IPV4(111, c, b, a);
            break;
        case 3:
            entry = ipv4_l3fwd_em_route_array[3];
            entry.key.dst_addr = RTE_IPV4(211, c, b, a);
            break;
        }
        struct data *data = NULL;
        int tbl_index = onvm_ft_add_key(h, &entry.key, (char **)&data);
        data->if_out = entry.if_out;
        if (tbl_index < 0)
            rte_exit(EXIT_FAILURE, "Unable to add entry %u\n", i);
        printf("\nAdding key:");
        _onvm_ft_print_key(&entry.key);
    }
    printf("Hash: Adding 0x%x keys\n", nr_flow);
}

uint16_t
em_get_ipv4_dst_port(struct rte_mbuf *pkt, struct state_info *stats) {
    struct data *data = NULL;
    struct onvm_ft_ipv4_5tuple key;
    uint8_t port;

    int ret = onvm_ft_fill_key(&key, pkt);

    if (ret < 0)
            return pkt->port;

    int tbl_index = onvm_ft_lookup_key(stats->em_tbl, &key, (char **)&data);
    if (tbl_index < 0)
            return 1;
    port = data->if_out;
    return port;
}

int
setup_hash(struct state_info *stats) {
    stats->em_tbl = onvm_ft_create(L3FWD_HASH_ENTRIES, sizeof(struct ipv4_l3fwd_em_route));
    if (stats->em_tbl == NULL) {
            printf("Unable to create flow table");
            return -1;
    }
    if (stats->hash_entry_number != HASH_ENTRY_NUMBER_DEFAULT) {
        /* For testing hash matching with a large number of flows we
         * generate millions of IP 5-tuples with an incremented dst
         * address to initialize the hash table. */
        /* populate the ipv4 hash */
        populate_ipv4_many_flow_into_table(stats->em_tbl, stats->hash_entry_number);
    } else {
        /*
         * Use data in ipv4/ipv6 l3fwd lookup table
         * directly to initialize the hash table.
         */
        /* populate the ipv4 hash */
        populate_ipv4_few_flow_into_table(stats->em_tbl);
    }
    return 0;
}
