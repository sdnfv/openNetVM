#ifndef _ONVM_FLOW_DIR_H_
#define _ONVM_FLOW_DIR_H_

#include "common.h"
#include "onvm_flow_table.h"

extern struct onvm_ft *sdn_ft;
extern struct onvm_ft **sdn_ft_p;

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
int onvm_flow_dir_init(void);
int onvm_flow_dir_nf_init(void);
int onvm_flow_dir_get_pkt(struct rte_mbuf* pkt, struct onvm_flow_entry **flow_entry);
int onvm_flow_dir_add_pkt(struct rte_mbuf* pkt, struct onvm_flow_entry **flow_entry);
/* delete the flow dir entry, but do not free the service chain (useful if a service chain is pointed to by several different flows */
int onvm_flow_dir_del_pkt(struct rte_mbuf* pkt);
/* Delete the flow dir entry and free the service chain */
int onvm_flow_dir_del_and_free_pkt(struct rte_mbuf* pkt);
int onvm_flow_dir_get_key(struct onvm_ft_ipv4_5tuple* key, struct onvm_flow_entry **flow_entry);
int onvm_flow_dir_add_key(struct onvm_ft_ipv4_5tuple* key, struct onvm_flow_entry **flow_entry);
int onvm_flow_dir_del_key(struct onvm_ft_ipv4_5tuple* key);
int onvm_flow_dir_del_and_free_key(struct onvm_ft_ipv4_5tuple* key);
#endif // _ONVM_FLOW_DIR_H_

