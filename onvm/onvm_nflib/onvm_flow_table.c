/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2016 George Washington University
 *            2015-2016 University of California Riverside
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
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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
 * onvm_flow_table.c - a generic flow table
 ********************************************************************/

#include <rte_ether.h>
#include <rte_hash.h>
#include <rte_lcore.h>
#include <rte_cycles.h>
#include <rte_malloc.h>

#include "onvm_flow_table.h"

uint8_t rss_symmetric_key[40] = { 0x6d, 0x5a, 0x6d, 0x5a,
                                     0x6d, 0x5a, 0x6d, 0x5a,
                                     0x6d, 0x5a, 0x6d, 0x5a,
                                     0x6d, 0x5a, 0x6d, 0x5a,
                                     0x6d, 0x5a, 0x6d, 0x5a,
                                     0x6d, 0x5a, 0x6d, 0x5a,
                                     0x6d, 0x5a, 0x6d, 0x5a,
                                     0x6d, 0x5a, 0x6d, 0x5a,
                                     0x6d, 0x5a, 0x6d, 0x5a,
                                     0x6d, 0x5a, 0x6d, 0x5a,};

/* Create a new flow table made of an rte_hash table and a fixed size
 * data array for storing values. Only supports IPv4 5-tuple lookups. */
struct onvm_ft*
onvm_ft_create(int cnt, int entry_size) {
        struct rte_hash* hash;
        struct onvm_ft* ft;
        struct rte_hash_parameters ipv4_hash_params = {
            .name = NULL,
            .entries = cnt,
            .key_len = sizeof(struct onvm_ft_ipv4_5tuple),
            .hash_func = NULL,
            .hash_func_init_val = 0,
        };

        char s[64];
        /* create ipv4 hash table. use core number and cycle counter to get a unique name. */
        ipv4_hash_params.name = s;
        ipv4_hash_params.socket_id = rte_socket_id();
        snprintf(s, sizeof(s), "onvm_ft_%d-%"PRIu64, rte_lcore_id(), rte_get_tsc_cycles());
        hash = rte_hash_create(&ipv4_hash_params);
        if (hash == NULL) {
                return NULL;
        }
        ft = (struct onvm_ft*)rte_calloc("table", 1, sizeof(struct onvm_ft), 0);
        if (ft == NULL) {
                rte_hash_free(hash);
                return NULL;
        }
        ft->hash = hash;
        ft->cnt = cnt;
        ft->entry_size = entry_size;
        /* Create data array for storing values */
        ft->data = rte_calloc("entry", cnt, entry_size, 0);
        if (ft->data == NULL) {
                rte_hash_free(hash);
                rte_free(ft);
                return NULL;
        }
        return ft;
}

/* Add an entry in flow table and set data to point to the new value.
Returns:
 index in the array on success
 -EPROTONOSUPPORT if packet is not ipv4.
 -EINVAL if the parameters are invalid.
 -ENOSPC if there is no space in the hash for this key.
*/
int
onvm_ft_add_pkt(struct onvm_ft* table, struct rte_mbuf *pkt, char** data) {
        int32_t tbl_index;
        struct onvm_ft_ipv4_5tuple key;
        int err;

        err = onvm_ft_fill_key(&key, pkt);
        if (err < 0) {
                return err;
        }
        tbl_index = rte_hash_add_key_with_hash(table->hash, (const void *)&key, pkt->hash.rss);
        if (tbl_index >= 0) {
        	*data = &table->data[tbl_index*table->entry_size];
        }
        return tbl_index;
}

/* Lookup an entry in flow table and set data to point to the value.
   Returns:
    index in the array on success
    -ENOENT if the key is not found.
    -EINVAL if the parameters are invalid.
*/
int
onvm_ft_lookup_pkt(struct onvm_ft* table, struct rte_mbuf *pkt, char** data) {
        int32_t tbl_index;
        struct onvm_ft_ipv4_5tuple key;
        int ret;

        ret = onvm_ft_fill_key(&key, pkt);
        if (ret < 0) {
                return ret;
        }
        tbl_index = rte_hash_lookup_with_hash(table->hash, (const void *)&key, pkt->hash.rss);
        if (tbl_index >= 0) {
                *data = onvm_ft_get_data(table, tbl_index);
        }
        return tbl_index;
}

/* Removes an entry from the flow table
   Returns:
    A positive value that can be used by the caller as an offset into an array of user data. This value is unique for this key, and is the same value that was returned when the key was added.
    -ENOENT if the key is not found.
    -EINVAL if the parameters are invalid.
*/
int32_t
onvm_ft_remove_pkt(struct onvm_ft *table, struct rte_mbuf *pkt)
{
        struct onvm_ft_ipv4_5tuple key;
        int ret;

        ret = onvm_ft_fill_key(&key, pkt);
        if (ret < 0) {
                return ret;
        }
        return rte_hash_del_key_with_hash(table->hash, (const void *)&key, pkt->hash.rss);
}

int
onvm_ft_add_key(struct onvm_ft* table, struct onvm_ft_ipv4_5tuple *key, char** data) {
        int32_t tbl_index;
        uint32_t softrss;

        softrss = onvm_softrss(key);

        tbl_index = rte_hash_add_key_with_hash(table->hash, (const void *)key, softrss);
        if (tbl_index >= 0) {
                *data = onvm_ft_get_data(table, tbl_index);
        }

        return tbl_index;
}

int
onvm_ft_lookup_key(struct onvm_ft* table, struct onvm_ft_ipv4_5tuple *key, char** data) {
        int32_t tbl_index;
	uint32_t softrss;

	softrss = onvm_softrss(key);

        tbl_index = rte_hash_lookup_with_hash(table->hash, (const void *)key, softrss);
	if (tbl_index >= 0) {
                *data = onvm_ft_get_data(table, tbl_index);
        }

	return tbl_index;
}

int32_t
onvm_ft_remove_key(struct onvm_ft *table, struct onvm_ft_ipv4_5tuple *key)
{
        return rte_hash_del_key(table->hash, (const void *)key);
}

/* Iterate through the hash table, returning key-value pairs.
   Parameters:
     key: Output containing the key where current iterator was pointing at
     data: Output containing the data associated with key. Returns NULL if data was not stored.
     next: Pointer to iterator. Should be 0 to start iterating the hash table. Iterator is incremented after each call of this function.
   Returns:
     Position where key was stored, if successful.
    -EINVAL if the parameters are invalid.
    -ENOENT if end of the hash table.
 */
int32_t
onvm_ft_iterate(struct onvm_ft *table, const void **key, void **data, uint32_t *next)
{
        return rte_hash_iterate(table->hash, key, data, next);
}

/* Clears a flow table and frees associated memory */
void
onvm_ft_free(struct onvm_ft *table)
{
        rte_hash_reset(table->hash);
        rte_hash_free(table->hash);
}
