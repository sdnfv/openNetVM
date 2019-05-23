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
 * onvm_flow_dir.c - flow director APIs
 ********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include "onvm_common.h"
#include "onvm_flow_table.h"
#include "onvm_flow_dir.h"

//#define LIST_FLOW_ENTRIES     //enable to list (print) the bottleneck chain entries.
#define NO_FLAGS 0


struct onvm_ft *sdn_ft;
struct onvm_ft **sdn_ft_p;

void
onvm_flow_dir_set_index(void) {

        if(sdn_ft) {
                uint32_t tbl_index = 0;
                for (; tbl_index < SDN_FT_ENTRIES; tbl_index++)
                {
                        struct onvm_flow_entry *flow_entry = (struct onvm_flow_entry *)&sdn_ft->data[tbl_index*sdn_ft->entry_size];
                        flow_entry->entry_index = tbl_index;
                }
        }
        return ;
}

int onvm_flow_dir_reset_entry(struct onvm_flow_entry *flow_entry) {

        if(flow_entry) {
                uint64_t ft_index = flow_entry->entry_index;
                memset(flow_entry,0,sizeof(struct onvm_flow_entry));
                flow_entry->entry_index = ft_index;
                return (int)ft_index;
        }
        return 0;
}

int
onvm_flow_dir_init(void)
{
	const struct rte_memzone *mz_ftp;

	    sdn_ft = onvm_ft_create(SDN_FT_ENTRIES, sizeof(struct onvm_flow_entry));
        if(sdn_ft == NULL) {
                rte_exit(EXIT_FAILURE, "Unable to create flow table\n");
        }
        mz_ftp = rte_memzone_reserve(MZ_FTP_INFO, sizeof(struct onvm_ft *),
                                  rte_socket_id(), NO_FLAGS);
        if (mz_ftp == NULL) {
                rte_exit(EXIT_FAILURE, "Canot reserve memory zone for flow table pointer\n");
        }
        memset(mz_ftp->addr, 0, sizeof(struct onvm_ft *));
        sdn_ft_p = mz_ftp->addr;
        *sdn_ft_p = sdn_ft;

    onvm_flow_dir_set_index();
	return 0;
}

int
onvm_flow_dir_nf_init(void)
{
	const struct rte_memzone *mz_ftp;
        struct onvm_ft **ftp;

        mz_ftp = rte_memzone_lookup(MZ_FTP_INFO);
        if (mz_ftp == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get table pointer\n");
        ftp = mz_ftp->addr;
        sdn_ft = *ftp;

	return 0;
}

int
onvm_flow_dir_get_pkt( struct rte_mbuf *pkt, struct onvm_flow_entry **flow_entry){
	int ret;
	ret = onvm_ft_lookup_pkt(sdn_ft, pkt, (char **)flow_entry);

	return ret;
}

int
onvm_flow_dir_add_pkt(struct rte_mbuf *pkt, struct onvm_flow_entry **flow_entry){
	int ret;
       	ret = onvm_ft_add_pkt(sdn_ft, pkt, (char**)flow_entry);

	return ret;
}

int
onvm_flow_dir_del_pkt(struct rte_mbuf* pkt){
	int ret;
	struct onvm_flow_entry *flow_entry;
	int ref_cnt;

        ret = onvm_flow_dir_get_pkt(pkt, &flow_entry);
	if (ret >= 0) {
		ref_cnt = flow_entry->sc->ref_cnt--;
		if (ref_cnt <= 0) {
			ret = onvm_flow_dir_del_and_free_pkt(pkt);
		}
	}

	return ret;
}

int
onvm_flow_dir_del_and_free_pkt(struct rte_mbuf *pkt){
	int ret;
	struct onvm_flow_entry *flow_entry;

	ret = onvm_flow_dir_get_pkt(pkt, &flow_entry);
	if (ret >= 0) {
		rte_free(flow_entry->sc); //modification mode to avoid releasing sc entry as it can be multiplexe  across different flow entries.
		rte_free(flow_entry->key);
		ret = onvm_ft_remove_pkt(sdn_ft, pkt);
	}

	return ret;
}

int
onvm_flow_dir_get_key(struct onvm_ft_ipv4_5tuple *key, struct onvm_flow_entry **flow_entry){
	int ret;
        ret = onvm_ft_lookup_key(sdn_ft, key, (char **)flow_entry);

        return ret;
}

int
onvm_flow_dir_add_key(struct onvm_ft_ipv4_5tuple *key, struct onvm_flow_entry **flow_entry){
        int ret;
        ret = onvm_ft_add_key(sdn_ft, key, (char**)flow_entry);

        return ret;
}

int
onvm_flow_dir_del_key(struct onvm_ft_ipv4_5tuple *key){
        int ret;
        struct onvm_flow_entry *flow_entry;
        int ref_cnt;

        ret = onvm_flow_dir_get_key(key, &flow_entry);
        if (ret >= 0) {
                ref_cnt = flow_entry->sc->ref_cnt--;
                if (ref_cnt <= 0) {
                        ret = onvm_flow_dir_del_and_free_key(key);
                }
        }

        return ret;
}

int
onvm_flow_dir_del_and_free_key(struct onvm_ft_ipv4_5tuple *key){
        int ret;
        struct onvm_flow_entry *flow_entry;

        ret = onvm_flow_dir_get_key(key, &flow_entry);
        if (ret >= 0) {
                //ret = onvm_ft_remove_key(sdn_ft, key); // This function keeps crashing
                rte_free(flow_entry->sc); //Modification to avoid releasing the service chain which can be multiplexed acrossdfferent flow entries
                flow_entry->sc=NULL;      // Need separate call to release the service chain as the api onvm_fc_create() have onvm_fc_release()
                rte_free(flow_entry->key);
                flow_entry->key=NULL;
        }

        return ret;
}
void onvm_flow_dir_print_stats__old(void);
void
onvm_flow_dir_print_stats__old(void) {

        if(sdn_ft) {
                int32_t tbl_index = 0;
                uint32_t active_chains = 0;
                uint32_t mapped_chains = 0;
                uint32_t bottlnecked_chains=0;
                for (; tbl_index < SDN_FT_ENTRIES; tbl_index++)
                {
                        struct onvm_flow_entry *flow_entry = (struct onvm_flow_entry *)&sdn_ft->data[tbl_index*sdn_ft->entry_size];
                        if (flow_entry && flow_entry->sc && flow_entry->sc->chain_length) {
                                active_chains+=1;
#ifdef ENABLE_NF_BACKPRESSURE
#ifdef NF_BACKPRESSURE_APPROACH_2
                                if(flow_entry->sc->nf_instance_id[1]) {
                                        mapped_chains+=1;
                                }
#endif
#endif
                        }
                        else continue;
#ifdef ENABLE_NF_BACKPRESSURE
                        if (flow_entry->sc->highest_downstream_nf_index_id) {
                                int i =0;
                                printf ("OverflowStatus [(binx=%d, %d),(nfid=%d),(scl=%d)::", flow_entry->sc->highest_downstream_nf_index_id, flow_entry->idle_timeout, flow_entry->sc->ref_cnt, flow_entry->sc->chain_length );
                                for(i=1;i<=flow_entry->sc->chain_length;++i)printf("[%d], ",flow_entry->sc->sc[i].destination);
                                if(flow_entry->key)
                                        printf ("Tuple:[SRC(%d:%d),DST(%d:%d), PROTO(%d)], \t", flow_entry->key->src_addr, rte_be_to_cpu_16(flow_entry->key->src_port), flow_entry->key->dst_addr, rte_be_to_cpu_16(flow_entry->key->dst_port), flow_entry->key->proto);
#ifdef NF_BACKPRESSURE_APPROACH_2
                                uint8_t nf_idx = 0;
                                for (; nf_idx < ONVM_MAX_CHAIN_LENGTH; nf_idx++) {
                                        printf("[%d: %d] \t", nf_idx, flow_entry->sc->nf_instance_id[nf_idx]);
                                }
#endif //NF_BACKPRESSURE_APPROACH_2
                                // printf("\n");
                                bottlnecked_chains++;
                        }
#endif  //ENABLE_NF_BACKPRESSURE
                }
                printf("Total chains: [%d], Bottleneck'd Chains: [%d], mapped chains: [%d]  \n", active_chains, bottlnecked_chains, mapped_chains);
        }

        return ;
}

static inline uint32_t get_index_of_sc(struct onvm_service_chain *sc, sc_entries_list *c_list, uint32_t max_entries) {
        uint32_t free_index = max_entries;
        uint32_t i = 0;
        for (i=0; i<max_entries; i++) {
                if (c_list[i].sc) {
                        if(c_list[i].sc == sc) {
                                return i;
                        }
                }
                else {
                        free_index = ((i < free_index)? (i):(free_index));
                }
        }
        return free_index;
}

uint32_t
extract_active_service_chains(uint32_t *bft_count, sc_entries_list *c_list, uint32_t max_entries) {
        uint32_t active_fts = 0, bneck_fts=0;
        if(!c_list) return -1;
        if(sdn_ft) {
                int32_t tbl_index = 0;
                uint32_t s_inx = SDN_FT_ENTRIES;

                memset(c_list,0,sizeof(*c_list));

                for (; tbl_index < SDN_FT_ENTRIES; tbl_index++) {
                        s_inx = SDN_FT_ENTRIES;
                        struct onvm_flow_entry *flow_entry = (struct onvm_flow_entry *)&sdn_ft->data[tbl_index*sdn_ft->entry_size];
                        if (flow_entry->sc && flow_entry->sc->chain_length) {
                                active_fts+=1;
                                s_inx = get_index_of_sc(flow_entry->sc, c_list,max_entries);
                                if(s_inx < SDN_FT_ENTRIES) {
                                        c_list[s_inx].sc = flow_entry->sc;
                                        c_list[s_inx].sc_count+=1;
                                        //if(1 == c_list[s_inx].sc_count) c_list[s_inx].bneck_flag=0;

#ifdef ENABLE_NF_BACKPRESSURE
                                        //Count Bottleneck Flow Table entries only if requested!
                                        if (bft_count && flow_entry->sc->highest_downstream_nf_index_id) {
                                                bneck_fts++;
                                                if(s_inx < SDN_FT_ENTRIES) {
                                                        c_list[s_inx].bneck_flag+=1;
                                                }
#ifdef LIST_FLOW_ENTRIES
                                                int i =0;
                                                //printf ("OverflowStatus [(binx=%d, %d),(nfid=%d),(scl=%d)::", flow_entry->sc->highest_downstream_nf_index_id, flow_entry->idle_timeout, flow_entry->sc->ref_cnt, flow_entry->sc->chain_length );
                                                fprintf (stdout, "EXTRACT: OverflowStatus [(binx=%d), (scl=%d)::", flow_entry->sc->highest_downstream_nf_index_id, flow_entry->sc->chain_length );
                                                for(i=1;i<=flow_entry->sc->chain_length;++i)printf("[%d], ",flow_entry->sc->sc[i].destination);
                                                if(flow_entry->key)
                                                        fprintf (stdout, "Tuple:[SRC(%d:%d),DST(%d:%d), PROTO(%d)], \t", flow_entry->key->src_addr, rte_be_to_cpu_16(flow_entry->key->src_port), flow_entry->key->dst_addr, rte_be_to_cpu_16(flow_entry->key->dst_port), flow_entry->key->proto);
                                                fprintf(stdout,"\n");
#endif
                                        }
#endif  //ENABLE_NF_BACKPRESSURE
                                }
                        }
                        else continue;
                }
        }
        if(bft_count)*bft_count = bneck_fts;
         return active_fts;
}

void
onvm_flow_dir_print_stats(__attribute__((unused)) FILE *fp) {
#ifdef ENABLE_NF_BACKPRESSURE
        if(sdn_ft) {
                if(NULL == fp) {fp = stdout;}
                static sc_entries_list sc_l[SDN_FT_ENTRIES];
                uint32_t active_chains = 0, bneck_chains=0;
                uint32_t active_fts = 0, bneck_fts=0;
                uint32_t s_inx = SDN_FT_ENTRIES;

                active_fts = extract_active_service_chains(&bneck_fts,sc_l,SDN_FT_ENTRIES);

                for(s_inx=0; s_inx <SDN_FT_ENTRIES; s_inx++) {
                        if(sc_l[s_inx].sc) {
                                active_chains+=1;
                                if(sc_l[s_inx].bneck_flag) {
                                        bneck_chains+=1;
#ifdef LIST_FLOW_ENTRIES
                                        int i =0;
                                        //fprintf (fp,"(ft_count=%d),, overflowStatus (binx=%d),(nfid=%d), (scl=%d)::", sc_l[s_inx].bneck_flag, sc_l[s_inx].sc->highest_downstream_nf_index_id, sc_l[s_inx].sc->ref_cnt, sc_l[s_inx].sc->chain_length);
                                        fprintf (fp,"STATS: (ft_count=%d), overflowStatus (binx=%d), (scl=%d)::", sc_l[s_inx].bneck_flag, sc_l[s_inx].sc->highest_downstream_nf_index_id, sc_l[s_inx].sc->chain_length);
                                        for(i=1;i<=sc_l[s_inx].sc->chain_length;++i)printf("[%d], ",sc_l[s_inx].sc->sc[i].destination);
                                        fprintf(fp, "\n");
#endif
                                }
                        }
                }
                fprintf(fp, "Total Flow entries and chains: [%d, %d], Bottleneck'd Flow entries and Chains: [%d, %d], \n", active_fts, active_chains, bneck_fts, bneck_chains);
        }
#endif //ENABLE_NF_BACKPRESSURE
        return ;
}

int
onvm_flow_dir_clear_all_entries(void) {
        if(sdn_ft) {
                int32_t tbl_index = 0;
                uint32_t active_chains = 0;
                uint32_t cleared_chains = 0;
                for (; tbl_index < SDN_FT_ENTRIES; tbl_index++) {
                        struct onvm_flow_entry *flow_entry = (struct onvm_flow_entry *)&sdn_ft->data[tbl_index*sdn_ft->entry_size];
                        if (flow_entry && flow_entry->key) { //flow_entry->sc && flow_entry->sc->chain_length) {
                                active_chains+=1;
                                if (onvm_flow_dir_del_key(flow_entry->key) >=0) cleared_chains++;
                        }
                        //else continue;
                }
                printf("Total chains: [%d], cleared chains: [%d]  \n", active_chains, cleared_chains);
                return (int)(active_chains-cleared_chains);
        }
        return 0;
}




