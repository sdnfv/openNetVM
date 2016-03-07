#include <rte_ether.h>
#include <rte_hash.h>
#include <rte_lcore.h>
#include <rte_cycles.h>

#include "onvm_flow_table.h"

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
            .hash_func = onvm_ft_ipv4_hash_crc,
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
        ft = (struct onvm_ft*) calloc(1, sizeof(struct onvm_ft));
        if (ft == NULL) {
                rte_hash_free(hash);
                return NULL;
        }
        ft->hash = hash;
        ft->cnt = cnt;
        ft->entry_size = entry_size;
        /* Create data array for storing values */
        ft->data = calloc(cnt, entry_size);
        if (ft->data == NULL) {
                rte_hash_free(hash);
                free(ft);
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
onvm_ft_add_with_hash(struct onvm_ft* table, struct rte_mbuf *pkt, char** data) {
        int32_t tbl_index;
        struct onvm_ft_ipv4_5tuple key;
        int err;

        err = onvm_ft_fill_key(&key, pkt);
        if (err < 0) {
                return err;
        }
        tbl_index = rte_hash_add_key_with_hash(table->hash, (const void *)&key, pkt->hash.rss);
        // printf("added index %d rss %d port %d\n", tbl_index, pkt->hash.rss, pkt->port);
        // _onvm_ft_print_key(&key);
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
onvm_ft_lookup_with_hash(struct onvm_ft* table, struct rte_mbuf *pkt, char** data) {
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
onvm_ft_remove_with_hash(struct onvm_ft *table, struct rte_mbuf *pkt)
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
onvm_ft_add(struct onvm_ft* table, struct onvm_ft_ipv4_5tuple *key, char** data) {
        int32_t tbl_index;

        tbl_index = rte_hash_add_key(table->hash, (const void *)key);
        if (tbl_index < 0) {
                return tbl_index;
        }
        *data = &table->data[tbl_index * table->entry_size];

        return tbl_index;
}

int
onvm_ft_lookup(struct onvm_ft* table, struct onvm_ft_ipv4_5tuple *key, char** data) {
        int32_t tbl_index;

        tbl_index = rte_hash_lookup(table->hash, (const void *)key);
        if (tbl_index >= 0) {
                *data = onvm_ft_get_data(table, tbl_index);
                return 0;
        }
        else {
                return tbl_index;
        }
}

int32_t
onvm_ft_remove(struct onvm_ft *table, struct onvm_ft_ipv4_5tuple *key)
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

