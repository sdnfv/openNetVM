#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <rte_memory.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include "common.h"
#include "onvm_flow_table.h"
#include "onvm_flow_dir.h"

int 
onvm_flow_dir_get_with_hash(struct onvm_ft *table, struct rte_mbuf *pkt, struct onvm_flow_entry **flow_entry){
	int ret;
	ret = onvm_ft_lookup_with_hash(table, pkt, (char **)flow_entry);

	return ret;
} 

int
onvm_flow_dir_add_with_hash(struct onvm_ft *table, struct rte_mbuf *pkt, struct onvm_flow_entry **flow_entry){
	int ret;
       	ret = onvm_ft_add_with_hash(table, pkt, (char**)flow_entry);

	return ret;
}

int 
onvm_flow_dir_del_with_hash(struct onvm_ft *table, struct rte_mbuf* pkt){
	int ret; 
	struct onvm_flow_entry *flow_entry;
	int used;

        ret = onvm_flow_dir_get_with_hash(table, pkt, &flow_entry);
	if (ret >= 0) {
		used = flow_entry->sc->used--;
		if (used <= 0) {
			ret = onvm_flow_dir_del_and_free_with_hash(table, pkt);
		}
	}

	return ret;	
}

int 
onvm_flow_dir_del_and_free_with_hash(struct onvm_ft *table, struct rte_mbuf *pkt){
	int ret; 
	struct onvm_flow_entry *flow_entry;
	
	ret = onvm_flow_dir_get_with_hash(table, pkt, &flow_entry);
	if (ret >= 0) {
		rte_free(flow_entry->sc);
		rte_free(flow_entry->key);	
		ret = onvm_ft_remove_with_hash(table, pkt);
	}

	return ret;
}

int 
onvm_flow_dir_get(struct onvm_ft *table, struct onvm_ft_ipv4_5tuple *key, struct onvm_flow_entry **flow_entry){
	int ret;
        ret = onvm_ft_lookup(table, key, (char **)flow_entry);

        return ret;
}

int
onvm_flow_dir_add(struct onvm_ft *table, struct onvm_ft_ipv4_5tuple *key, struct onvm_flow_entry **flow_entry){
        int ret;
        ret = onvm_ft_add(table, key, (char**)flow_entry);

        return ret;
}

int 
onvm_flow_dir_del(struct onvm_ft *table, struct onvm_ft_ipv4_5tuple *key){
        int ret;    
        struct onvm_flow_entry *flow_entry;
        int used;

        ret = onvm_flow_dir_get(table, key, &flow_entry);
        if (ret >= 0) {
                used = flow_entry->sc->used--;
                if (used <= 0) {
                        ret = onvm_flow_dir_del_and_free(table, key);
                }
        }

        return ret;
}

int
onvm_flow_dir_del_and_free(struct onvm_ft *table, struct onvm_ft_ipv4_5tuple *key){
        int ret;
        struct onvm_flow_entry *flow_entry;

        ret = onvm_flow_dir_get(table, key, &flow_entry);
        if (ret >= 0) {
                rte_free(flow_entry->sc);
                rte_free(flow_entry->key);
                ret = onvm_ft_remove(table, key);
        }

        return ret;
}
