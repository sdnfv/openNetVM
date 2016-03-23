#ifndef _ONVM_FLOW_DIR_H_
#define _ONVM_FLOW_DIR_H_

#include "common.h"
#include "onvm_flow_table.h"

struct onvm_flow_entry {
        struct onvm_ft_ipv4_5tuple *key;
        struct onvm_service_chain *sc;
        uint64_t ref_cnt;
        uint16_t idle_timeout;
        uint16_t hard_timeout;
        uint64_t packet_count;
        uint64_t byte_count;
};

/* Get a pointer to the flow entry entry for this packet.
 * Returns:
 *  0        on success. *flow_entry points to this packet flow's flow entry
 *  -ENOENT  if flow has not been added to table. *flow_entry points to flow entry
 */
int onvm_flow_dir_get_with_hash(struct onvm_ft *table, struct rte_mbuf* pkt, struct onvm_flow_entry **flow_entry);
int onvm_flow_dir_add_with_hash(struct onvm_ft *table, struct rte_mbuf* pkt, struct onvm_flow_entry **flow_entry);
/* delete the flow dir entry, but do not free the service chain (useful if a service chain is pointed to by several different flows */
int onvm_flow_dir_del_with_hash(struct onvm_ft *table, struct rte_mbuf* pkt);
/* Delete the flow dir entry and free the service chain */
int onvm_flow_dir_del_and_free_with_hash(struct onvm_ft *table, struct rte_mbuf* pkt);
int onvm_flow_dir_get(struct onvm_ft *table, struct onvm_ft_ipv4_5tuple* key, struct onvm_flow_entry **flow_entry);
int onvm_flow_dir_add(struct onvm_ft *table, struct onvm_ft_ipv4_5tuple* key, struct onvm_flow_entry **flow_entry);
int onvm_flow_dir_del(struct onvm_ft *table, struct onvm_ft_ipv4_5tuple* key);
int onvm_flow_dir_del_and_free(struct onvm_ft *table, struct onvm_ft_ipv4_5tuple* key);
#endif // _ONVM_FLOW_DIR_H_

