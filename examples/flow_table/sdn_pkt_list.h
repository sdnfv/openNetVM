#ifndef _SDN_PKT_LIST_H_
#define _SDN_PKT_LIST_H_

#include <rte_mbuf.h>
#include "onvm_nflib.h"

struct sdn_pkt_list {
        struct sdn_pkt_entry *head;
        struct sdn_pkt_entry *tail;
        int flag;
};

struct sdn_pkt_entry {
        struct rte_mbuf *pkt;
        struct sdn_pkt_entry *next;
};

// FIXME: These functions should have return codes to indicate errors.

static inline void
sdn_pkt_list_init(struct sdn_pkt_list* list, struct rte_mbuf *pkt) {
        /* FIXME: check for malloc errors */
        list->head = (struct sdn_pkt_entry*) calloc(1, sizeof(struct sdn_pkt_entry));
        list->head->pkt = pkt;
        list->head->next = NULL;
        list->tail = list->head;
        list->flag = 0;
}

static inline void
sdn_pkt_list_add(struct sdn_pkt_list* list, struct rte_mbuf *pkt) {
        /* FIXME: check for malloc errors */
        struct sdn_pkt_entry* entry;
        entry = (struct sdn_pkt_entry*) calloc(1, sizeof(struct sdn_pkt_entry));
        entry->pkt = pkt;
        entry->next = NULL;
        list->tail->next = entry;
        list->tail = entry;
}

static inline void
sdn_pkt_list_set_flag(struct sdn_pkt_list* list) {
    	list->flag = 1;
}

static inline int
sdn_pkt_list_get_flag(struct sdn_pkt_list* list) {
    	return list->flag;
}

static inline void 
sdn_pkt_list_flush(struct sdn_pkt_list* list) {
	struct sdn_pkt_entry* entry;
	struct rte_mbuf *pkt;
	struct onvm_pkt_meta* meta;

	while(list->head) {
		entry = list->head;
		list->head = entry->next;			
		pkt = entry->pkt;
		meta = onvm_get_pkt_meta(pkt);
		meta->action = ONVM_NF_ACTION_NEXT;
		onvm_nf_return_pkt(pkt);
		free(entry);
	}

	list->flag = 0;
	list->head = NULL;
	list->tail = NULL;
}
#endif
