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
onvm_flow_dir_get_pkt(struct onvm_ft *table, struct rte_mbuf *pkt, struct onvm_flow_entry **flow_entry){
	int ret;
	ret = onvm_ft_lookup_pkt(table, pkt, (char **)flow_entry);

	return ret;
} 

int
onvm_flow_dir_add_pkt(struct onvm_ft *table, struct rte_mbuf *pkt, struct onvm_flow_entry **flow_entry){
	int ret;
       	ret = onvm_ft_add_pkt(table, pkt, (char**)flow_entry);

	return ret;
}

int 
onvm_flow_dir_del_pkt(struct onvm_ft *table, struct rte_mbuf* pkt){
	int ret; 
	struct onvm_flow_entry *flow_entry;
	int ref_cnt;

        ret = onvm_flow_dir_get_pkt(table, pkt, &flow_entry);
	if (ret >= 0) {
		ref_cnt = flow_entry->sc->ref_cnt--;
		if (ref_cnt <= 0) {
			ret = onvm_flow_dir_del_and_free_pkt(table, pkt);
		}
	}

	return ret;	
}

int 
onvm_flow_dir_del_and_free_pkt(struct onvm_ft *table, struct rte_mbuf *pkt){
	int ret; 
	struct onvm_flow_entry *flow_entry;
	
	ret = onvm_flow_dir_get_pkt(table, pkt, &flow_entry);
	if (ret >= 0) {
		rte_free(flow_entry->sc);
		rte_free(flow_entry->key);	
		ret = onvm_ft_remove_pkt(table, pkt);
	}

	return ret;
}

int 
onvm_flow_dir_get_key(struct onvm_ft *table, struct onvm_ft_ipv4_5tuple *key, struct onvm_flow_entry **flow_entry){
	int ret;
        ret = onvm_ft_lookup_key(table, key, (char **)flow_entry);

        return ret;
}

int
onvm_flow_dir_add_key(struct onvm_ft *table, struct onvm_ft_ipv4_5tuple *key, struct onvm_flow_entry **flow_entry){
        int ret;
        ret = onvm_ft_add_key(table, key, (char**)flow_entry);

        return ret;
}

int 
onvm_flow_dir_del_key(struct onvm_ft *table, struct onvm_ft_ipv4_5tuple *key){
        int ret;    
        struct onvm_flow_entry *flow_entry;
        int ref_cnt;

        ret = onvm_flow_dir_get_key(table, key, &flow_entry);
        if (ret >= 0) {
                ref_cnt = flow_entry->sc->ref_cnt--;
                if (ref_cnt <= 0) {
                        ret = onvm_flow_dir_del_and_free_key(table, key);
                }
        }

        return ret;
}

int
onvm_flow_dir_del_and_free_key(struct onvm_ft *table, struct onvm_ft_ipv4_5tuple *key){
        int ret;
        struct onvm_flow_entry *flow_entry;

        ret = onvm_flow_dir_get_key(table, key, &flow_entry);
        if (ret >= 0) {
                rte_free(flow_entry->sc);
                rte_free(flow_entry->key);
                ret = onvm_ft_remove_key(table, key);
        }

        return ret;
}
