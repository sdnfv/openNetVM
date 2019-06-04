/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2019 George Washington University
 *            2015-2019 University of California Riverside
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
 * onvm_flow_table.h - a generic flow table
 ********************************************************************/

#ifndef _ONVM_FLOW_TABLE_H_
#define _ONVM_FLOW_TABLE_H_

#include <rte_common.h>
#include <rte_ip.h>
#include <rte_mbuf.h>
#include <rte_tcp.h>
#include <rte_thash.h>
#include <rte_udp.h>
#include <string.h>
#include "onvm_common.h"
#include "onvm_pkt_helper.h"

extern uint8_t rss_symmetric_key[40];

#ifdef RTE_MACHINE_CPUFLAG_SSE4_2
#include <rte_hash_crc.h>
#define DEFAULT_HASH_FUNC rte_hash_crc
#else
#include <rte_jhash.h>
#define DEFAULT_HASH_FUNC rte_jhash
#endif

struct onvm_ft {
        struct rte_hash *hash;
        char *data;
        int cnt;
        int entry_size;
};

struct onvm_ft_ipv4_5tuple {
        uint32_t src_addr;
        uint32_t dst_addr;
        uint16_t src_port;
        uint16_t dst_port;
        uint8_t proto;
};

/* from l2_forward example, but modified to include port. This should
 * be automatically included in the hash functions since it hashes
 * the struct in 4byte chunks. */
union ipv4_5tuple_host {
        struct {
                uint8_t pad0;
                uint8_t proto;
                uint16_t virt_port;
                uint32_t ip_src;
                uint32_t ip_dst;
                uint16_t port_src;
                uint16_t port_dst;
        };
        __m128i xmm;
};

struct onvm_ft *
onvm_ft_create(int cnt, int entry_size);

int
onvm_ft_add_pkt(struct onvm_ft *table, struct rte_mbuf *pkt, char **data);

int
onvm_ft_lookup_pkt(struct onvm_ft *table, struct rte_mbuf *pkt, char **data);

int32_t
onvm_ft_remove_pkt(struct onvm_ft *table, struct rte_mbuf *pkt);

int
onvm_ft_add_key(struct onvm_ft *table, struct onvm_ft_ipv4_5tuple *key, char **data);

int
onvm_ft_lookup_key(struct onvm_ft *table, struct onvm_ft_ipv4_5tuple *key, char **data);

int32_t
onvm_ft_remove_key(struct onvm_ft *table, struct onvm_ft_ipv4_5tuple *key);

int32_t
onvm_ft_iterate(struct onvm_ft *table, const void **key, void **data, uint32_t *next);

void
onvm_ft_free(struct onvm_ft *table);

/* TODO(@sdnfv): Add function to calculate hash and then make lookup/get
 * have versions with precomputed hash values */
// hash_sig_t
// onvm_ft_hash(struct rte_mbuf *buf);

static inline void
_onvm_ft_print_key(struct onvm_ft_ipv4_5tuple *key) {
        printf("IP: %" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8, key->src_addr & 0xFF, (key->src_addr >> 8) & 0xFF,
               (key->src_addr >> 16) & 0xFF, (key->src_addr >> 24) & 0xFF);
        printf("-%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8 " ", key->dst_addr & 0xFF, (key->dst_addr >> 8) & 0xFF,
               (key->dst_addr >> 16) & 0xFF, (key->dst_addr >> 24) & 0xFF);
        printf("Port: %d %d Proto: %d\n", key->src_port, key->dst_port, key->proto);
}

static inline char *
onvm_ft_get_data(struct onvm_ft *table, int32_t index) {
        return &table->data[index * table->entry_size];
}

static inline int
onvm_ft_fill_key(struct onvm_ft_ipv4_5tuple *key, struct rte_mbuf *pkt) {
        struct ipv4_hdr *ipv4_hdr;
        struct tcp_hdr *tcp_hdr;
        struct udp_hdr *udp_hdr;

        if (unlikely(!onvm_pkt_is_ipv4(pkt))) {
                return -EPROTONOSUPPORT;
        }
        ipv4_hdr = onvm_pkt_ipv4_hdr(pkt);
        memset(key, 0, sizeof(struct onvm_ft_ipv4_5tuple));
        key->proto = ipv4_hdr->next_proto_id;
        key->src_addr = ipv4_hdr->src_addr;
        key->dst_addr = ipv4_hdr->dst_addr;
        if (key->proto == IP_PROTOCOL_TCP) {
                tcp_hdr = onvm_pkt_tcp_hdr(pkt);
                key->src_port = tcp_hdr->src_port;
                key->dst_port = tcp_hdr->dst_port;
        } else if (key->proto == IP_PROTOCOL_UDP) {
                udp_hdr = onvm_pkt_udp_hdr(pkt);
                key->src_port = udp_hdr->src_port;
                key->dst_port = udp_hdr->dst_port;
        } else {
                key->src_port = 0;
                key->dst_port = 0;
        }
        return 0;
}

static inline int
onvm_ft_fill_key_symmetric(struct onvm_ft_ipv4_5tuple *key, struct rte_mbuf *pkt) {
        if (onvm_ft_fill_key(key, pkt) < 0) {
                return -EPROTONOSUPPORT;
        }

        if (key->dst_addr > key->src_addr) {
                uint32_t temp = key->dst_addr;
                key->dst_addr = key->src_addr;
                key->src_addr = temp;
        }

        if (key->dst_port > key->src_port) {
                uint16_t temp = key->dst_port;
                key->dst_port = key->src_port;
                key->src_port = temp;
        }

        return 0;
}

/* Hash a flow key to get an int. From L3 fwd example */
static inline uint32_t
onvm_ft_ipv4_hash_crc(const void *data, __rte_unused uint32_t data_len, uint32_t init_val) {
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
#else  /* RTE_MACHINE_CPUFLAG_SSE4_2 */
        init_val = rte_jhash_1word(t, init_val);
        init_val = rte_jhash_1word(k->ip_src, init_val);
        init_val = rte_jhash_1word(k->ip_dst, init_val);
        init_val = rte_jhash_1word(*p, init_val);
#endif /* RTE_MACHINE_CPUFLAG_SSE4_2 */
        return (init_val);
}

/*software caculate RSS*/
static inline uint32_t
onvm_softrss(struct onvm_ft_ipv4_5tuple *key) {
        union rte_thash_tuple tuple;
        uint8_t rss_key_be[RTE_DIM(rss_symmetric_key)];
        uint32_t rss_l3l4;

        rte_convert_rss_key((uint32_t *)rss_symmetric_key, (uint32_t *)rss_key_be, RTE_DIM(rss_symmetric_key));

        tuple.v4.src_addr = rte_be_to_cpu_32(key->src_addr);
        tuple.v4.dst_addr = rte_be_to_cpu_32(key->dst_addr);
        tuple.v4.sport = rte_be_to_cpu_16(key->src_port);
        tuple.v4.dport = rte_be_to_cpu_16(key->dst_port);

        rss_l3l4 = rte_softrss_be((uint32_t *)&tuple, RTE_THASH_V4_L4_LEN, rss_key_be);

        return rss_l3l4;
}

#endif  // _ONVM_FLOW_TABLE_H_
