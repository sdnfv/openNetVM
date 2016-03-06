


#include <rte_ether.h>
#include <rte_hash.h>
#include <rte_lcore.h>

#include "onvm_flow_table.h"

/* Number of flow tables created so far. */
int onvm_ft_num_tables = 0;

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
        /* create ipv4 hash table */
        /* FIXME: Each HT needs a unique name, this could be a race condition */
        snprintf(s, sizeof(s), "onvm_ft_%d", onvm_ft_num_tables++);
        ipv4_hash_params.name = s;
        ipv4_hash_params.socket_id = rte_socket_id();
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
 0 on success
 -EPROTONOSUPPORT if packet is not ipv4.
 -EINVAL if the parameters are invalid.
 -ENOSPC if there is no space in the hash for this key.
*/
int
onvm_ft_add_with_hash(struct onvm_ft* table, struct rte_mbuf *pkt, char** data, int *index) {
        int32_t tbl_index;
        struct onvm_ft_ipv4_5tuple key;
        int err;

        err = _onvm_ft_fill_key(&key, pkt);
        if (err < 0) {
                return err;
        }
        tbl_index = rte_hash_add_key_with_hash(table->hash, (const void *)&key, pkt->hash.rss);
        printf("added index %d rss %d port %d\n", tbl_index, pkt->hash.rss, pkt->port);
        _onvm_ft_print_key(&key);
        if (tbl_index < 0) {
		*index = -1;
                return tbl_index;
        }
	*index = tbl_index;
        *data = &table->data[tbl_index*table->entry_size];
        return 0;
}

/* Lookup an entry in flow table and set data to point to the value.
   Returns:
    0 on success
    -ENOENT if the key is not found.
    -EINVAL if the parameters are invalid.
*/
int
onvm_ft_lookup_with_hash(struct onvm_ft* table, struct rte_mbuf *pkt, char** data, int *index) {
        int32_t tbl_index;
        struct onvm_ft_ipv4_5tuple key;
        int ret;

        ret = _onvm_ft_fill_key(&key, pkt);
        if (ret < 0) {
		*index = -1;
                return ret;
        }
        tbl_index = rte_hash_lookup_with_hash(table->hash, (const void *)&key, pkt->hash.rss);
        printf("lookup index %d rss %d port %d\n", tbl_index, pkt->hash.rss, pkt->port);
        _onvm_ft_print_key(&key);
        if (tbl_index >= 0) {
		*index = tbl_index;
                *data = &table->data[tbl_index*table->entry_size];
                return 0;
        }
        else {
		*index = -1;
		return tbl_index;
	}
}

int
onvm_ft_add(struct onvm_ft* table, struct onvm_flow_key *key, char** data) {
	int32_t tbl_index;

	tbl_index = rte_hash_add_key(table->hash, (const void *)key);
	if (tbl_index < 0) {
		return tbl_index;
	}
	*data = &table->data[tbl_index * table->entry_size];

	return 0;
}

int onvm_ft_lookup(struct onvm_ft* table, struct onvm_flow_key *key, char** data) {
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
