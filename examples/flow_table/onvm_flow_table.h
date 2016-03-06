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
 * onvm_flow_table.h - a generic flow table
 ********************************************************************/

#ifndef _ONVM_FLOW_TABLE_H_
#define _ONVM_FLOW_TABLE_H_

#include <rte_hash_crc.h>
#include <rte_mbuf.h>
#include <rte_common.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include "onvm_pkt_helper.h"
#include "common.h"

#ifdef RTE_MACHINE_CPUFLAG_SSE4_2
#include <rte_hash_crc.h>
#define DEFAULT_HASH_FUNC       rte_hash_crc
#else
#include <rte_jhash.h>
#define DEFAULT_HASH_FUNC       rte_jhash
#endif


struct onvm_ft {
        struct rte_hash* hash;
        char* data;
        int cnt;
        int entry_size;
};

struct onvm_ft_ipv4_5tuple {
        uint32_t ip_src;
        uint32_t ip_dst;
        uint16_t port_src;
        uint16_t port_dst;
        uint8_t  proto;
};

/* from l2_forward example, but modified to include port. This should
 * be automatically included in the hash functions since it hashes
 * the struct in 4byte chunks. */
union ipv4_5tuple_host {
        struct {
                uint8_t  pad0;
                uint8_t  proto;
                uint16_t virt_port;
                uint32_t ip_src;
                uint32_t ip_dst;
                uint16_t port_src;
                uint16_t port_dst;
        };
        __m128i xmm;
};

struct onvm_flow_key {
	uint16_t in_port;
	uint8_t src_eth[6];
	uint8_t dst_eth[6];
	uint16_t ether_type;
	uint32_t src_ip;
	uint32_t dst_ip;
	uint8_t proto;
	uint16_t src_port;
	uint16_t dst_port;
};

struct onvm_flow_entry {
	struct onvm_flow_key *key;
	struct onvm_service_chain *sc;
	uint64_t used;
	uint64_t created;
	uint16_t idle_timeout;
	uint16_t hard_timeout;
	uint64_t packet_count;
	uint64_t byte_count;
};


struct onvm_ft*
onvm_ft_create(int cnt, int entry_size);

int
onvm_ft_add_with_hash(struct onvm_ft* table, struct rte_mbuf *pkt, char** data, int *index);

int
onvm_ft_lookup_with_hash(struct onvm_ft* table, struct rte_mbuf *pkt, char** data, int *index);

int 
onvm_ft_add(struct onvm_ft* table, struct onvm_flow_key *key, char** data);

int 
onvm_ft_lookup(struct onvm_ft* table, struct onvm_flow_key *key, char** data);

/* TODO: Add function to calculate hash and then make lookup/get
 * have versions with precomputed hash values */
//hash_sig_t
//onvm_ft_hash(struct rte_mbuf *buf);

static inline void
_onvm_ft_print_key(struct onvm_ft_ipv4_5tuple *key) {
        printf("IP: %d %d Port: %d %d Proto: %d\n", key->ip_src, key->ip_dst,
                key->port_src, key->port_dst, key->proto);
}

static inline char*
onvm_ft_get_data(struct onvm_ft* table, int32_t index) {
	return &table->data[index*table->entry_size];
}

static inline int
_onvm_ft_fill_key(struct onvm_ft_ipv4_5tuple *key, struct rte_mbuf *pkt) {
        struct ipv4_hdr *ipv4_hdr;
        struct tcp_hdr *tcp_hdr;

        if (unlikely(!onvm_pkt_is_ipv4(pkt))) {
                return -EPROTONOSUPPORT;
        }
        ipv4_hdr = onvm_pkt_ipv4_hdr(pkt);
        tcp_hdr = onvm_pkt_tcp_hdr(pkt);
        memset(key, 0, sizeof(struct onvm_ft_ipv4_5tuple));
        key->proto  = ipv4_hdr->next_proto_id;
        key->ip_src = RTE_MIN(ipv4_hdr->src_addr, ipv4_hdr->dst_addr);
        key->ip_dst = RTE_MAX(ipv4_hdr->src_addr, ipv4_hdr->dst_addr);
        if (tcp_hdr != NULL) {
                key->port_src = RTE_MIN(tcp_hdr->src_port, tcp_hdr->dst_port);
                key->port_dst = RTE_MAX(tcp_hdr->src_port, tcp_hdr->dst_port);
        }
        else {
                key->port_src = 0;
                key->port_dst = 0;
        }
        return 0;
}

/* Hash a flow key to get an int. From L3 fwd example */
static inline uint32_t
onvm_ft_ipv4_hash_crc(const void *data, __rte_unused uint32_t data_len,
        uint32_t init_val)
{
        const union ipv4_5tuple_host *k;
        uint32_t t;
        const uint32_t *p;

        k = data;
        t = k->proto;
        p = (const uint32_t *)&k->port_src;

#ifdef RTE_MACHINE_CPUFLAG_SSE4_2
        init_val = rte_hash_crc_4byte(t, init_val);
        init_val = rte_hash_crc_4byte(k->ip_src, init_val);
        init_val = rte_hash_crc_4byte(k->ip_dst, init_val);
        init_val = rte_hash_crc_4byte(*p, init_val);
#else /* RTE_MACHINE_CPUFLAG_SSE4_2 */
        init_val = rte_jhash_1word(t, init_val);
        init_val = rte_jhash_1word(k->ip_src, init_val);
        init_val = rte_jhash_1word(k->ip_dst, init_val);
        init_val = rte_jhash_1word(*p, init_val);
#endif /* RTE_MACHINE_CPUFLAG_SSE4_2 */
        return (init_val);
}


#endif
