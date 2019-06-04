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
 * sdn_pkt_list.h - list operations
 ********************************************************************/

#ifndef _SDN_PKT_LIST_H_
#define _SDN_PKT_LIST_H_

#include <rte_mbuf.h>
#include "onvm_nflib.h"

struct sdn_pkt_list {
        struct sdn_pkt_entry* head;
        struct sdn_pkt_entry* tail;
        int flag;
        int counter;
};

struct sdn_pkt_entry {
        struct rte_mbuf* pkt;
        struct sdn_pkt_entry* next;
};

// FIXME: These functions should have return codes to indicate errors.

static inline void
sdn_pkt_list_init(struct sdn_pkt_list* list) {
        /* FIXME: check for malloc errors */
        list->head = NULL;
        list->tail = NULL;
        list->flag = 0;
        list->counter = 0;
}

static inline void
sdn_pkt_list_add(struct sdn_pkt_list* list, struct rte_mbuf* pkt) {
        list->counter++;
        /* FIXME: check for malloc errors */
        struct sdn_pkt_entry* entry;
        entry = (struct sdn_pkt_entry*)calloc(1, sizeof(struct sdn_pkt_entry));
        entry->pkt = pkt;
        entry->next = NULL;
        if (list->head == NULL) {
                list->head = entry;
                list->tail = list->head;
        } else {
                list->tail->next = entry;
                list->tail = entry;
        }
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
sdn_pkt_list_flush(struct onvm_nf* nf, struct sdn_pkt_list* list) {
        struct sdn_pkt_entry* entry;
        struct rte_mbuf* pkt;
        struct onvm_pkt_meta* meta;

        printf("list items:%d\n", list->counter);

        while (list->head) {
                entry = list->head;
                list->head = entry->next;
                pkt = entry->pkt;
                meta = onvm_get_pkt_meta(pkt);
                meta->action = ONVM_NF_ACTION_NEXT;
                meta->chain_index = 0;
                onvm_nflib_return_pkt(nf, pkt);
                free(entry);
                list->counter--;
        }

        list->flag = 0;
        list->head = NULL;
        list->tail = NULL;
        list->counter = 0;
}

#endif // _SDN_PKT_LIST_H_
