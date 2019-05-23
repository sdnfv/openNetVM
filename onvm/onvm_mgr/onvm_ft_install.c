/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2016 George Washington University
 *            2015-2016 University of California Riverside
 *            2010-2014 Intel Corporation. All rights reserved.
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
 ********************************************************************/


/******************************************************************************

                              onvm_special_nf0.c

       This file contains all functions related to NF management.

******************************************************************************/

#include "onvm_mgr.h"
#include "onvm_pkt.h"
#include "onvm_nf.h"

#include "onvm_pkt_helper.h"
#include "onvm_sc_common.h"
#include "onvm_sc_mgr.h"
//#include "onvm_flow_table.h"
//#include "onvm_flow_dir.h"


#include "onvm_ft_install.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>


/**************************Macros and Feature Definitions**********************/
/* List of Global Command Line Arguments */
typedef struct onvm_ft_args {
        const char* servicechain_file;      /* -s <service chain listings> */
        const char* ipv4rules_file;         /* -r <IPv45Tuple Flow Table entries> */
        const char* base_ip_addr;           /* -b <IPv45Tuple Base Ip Address> */
        uint32_t max_ip_addrs;              /* -m <Maximum number of IP Addresses> */
        uint32_t max_ft_rules;              /* -M <Maximum number of FT entries> */
        uint32_t rev_order;                 /* -o <Order of Service Chian for Flip key */
}onvm_ft_args_t;
//static const char *optString = ":s:r:b:m:M";

#define MAX_FLOW_TABLE_ENTRIES SDN_FT_ENTRIES
static onvm_ft_args_t globals = {
        .servicechain_file = "services.txt",
        .ipv4rules_file = "ipv4rules.txt",
        .base_ip_addr   = "10.0.0.1",
        .max_ip_addrs   = 10,
        .max_ft_rules   = MAX_FLOW_TABLE_ENTRIES,
        .rev_order      =0,         /* 0 = same order A->B->C => A-B->C; 1 = A->B->C => C-B->A */
};

/* Service Chain Related entries */
#define MAX_SERVICE_CHAINS 32
int schains[MAX_SERVICE_CHAINS][ONVM_MAX_CHAIN_LENGTH];
int alt_port[MAX_SERVICE_CHAINS];          //designate alternate backup port to be used to route traffic in case of failure on primary port
int max_service_chains=0;
struct onvm_ft_ipv4_5tuple ipv4_5tRules[MAX_FLOW_TABLE_ENTRIES];
uint32_t max_ft_entries=0;
//#define DEBUG_0

/*************************Local functions Declaration**************************/
static uint32_t get_ipv4_value(const char *ip_addr){

        if (NULL == ip_addr) {
                return 0;
        }
        struct sockaddr_in antelope;
        inet_aton(ip_addr, &antelope.sin_addr);
        #ifdef DEBUG_0
        printf("For IP:[%s] the IPv4 value is: [%x]\n", ip_addr, antelope.sin_addr.s_addr);
        #endif
        return rte_be_to_cpu_32(antelope.sin_addr.s_addr);

        /*
         * Alternate logic to generate internally:
         * "10.0.0.1"  => 10*2^24 + 0*2^16 + 0*2^8 + 1;
         */
        #ifdef DEBUG_0
        uint32_t ipv4_value = 0;
        in_addr_t ipv4 = inet_addr(ip_addr);
        ipv4_value = ipv4;

        int a=0,b=0,c=0,d=0;
        sscanf(ip_addr, "%d.%d.%d.%d", &a,&b,&c,&d);
        uint32_t ipv4_val2 = (a << 24) + (b << 16) + (c << 8) + d;
        uint32_t ipv4_val3 = (d << 24) + (c << 16) + (b << 8) + a;
        printf("For IP:[%s] the IPv4 values are: [%x: %x], [%x : %x] \n", ip_addr, ipv4_value, antelope.sin_addr.s_addr, ipv4_val2,  ipv4_val3);
        //exit(1);
        return rte_be_to_cpu_32(ipv4_val2);
        #endif
}
/******************Parse Files: Policies (Chains) and Preset IP Rules *********/
static void
parse_service_chains(int schains[][ONVM_MAX_CHAIN_LENGTH]) {
        FILE *fp = fopen(globals.servicechain_file, "rb");
        int i=0;

#ifdef DEBUG_0
        printf("parsing schains in file services.txt\n\n");
#endif
        if (fp == NULL) {
                fprintf(stderr, "Can not open services.txt file\n");
                return; //exit(1);
        }

        while (fp && !feof(fp)) {
                char line[64] = "";
                char svc[64] = "";
                if (fgets(line, 63, fp) == NULL) {
                        continue;
                }
#ifdef DEBUG_0
                printf("parsing schains in line[%s]\n", line);
#endif

                int llen = strlen(line);
                int slen = 0;
                int svc_index = 0;
                int added = 0;
                for(i=0; i< llen; i++) {
                        if(( (',' == line[i])  || ('\0' == line[i]) || ('\n' == line[i]) ) && (slen > 0)) {
                                svc[slen++] = '\0';
                                int svc_id = atoi(svc);
                                if (svc_id > 0) {
                                        schains[max_service_chains][svc_index++] = svc_id;
                                }
                                slen = 0;
                                added+=1;
                        }
                        if (line[i] != '\0' && line[i] != ',') {
                                svc[slen++] = line[i];
                        }
                        if(ONVM_MAX_CHAIN_LENGTH <= svc_index) {
#ifdef DEBUG_0
                                printf("service chain reached maximum length: %d",(int)svc_index);
#endif
                                svc_index = 0;
                                break;
                        }
                }
                if (added) {
                        max_service_chains++;
                        added = 0;
                }
                if (MAX_SERVICE_CHAINS <= max_service_chains ) {
#ifdef DEBUG_0
                        printf("Reached the limit on allowed num of service chains [%d]!", max_service_chains);
#endif
                        break;
                }
        }
        if (fp) {
                fclose(fp);
        }
//#define DEBUG
#ifdef DEBUG
        int j = 0;
        printf("Read Service List!\n");
        for(i=0; i< max_service_chains; i++){
                for(j=0; j< ONVM_MAX_CHAIN_LENGTH; j++){
                        printf("%d ", schains[i][j]);
                }
                printf("\n");
        }
        printf("\n\n************\n\n");
#else
        printf("Total Service Chains = [%d]", max_service_chains);
#endif
        return;
}

static void
parse_ipv4_5t_rules(void) {
        FILE *fp = fopen(globals.ipv4rules_file, "rb");
        uint32_t i=0;
#ifdef DEBUG_0
        printf("parsing schains in file ipv4rules_file.txt\n\n");
#endif
        if (fp == NULL) {
                fprintf(stderr, "Can not open ipv4rules_file.txt file\n");
                return; //exit(1);
        }
        while (fp && !feof(fp)) {
                char line[256] = "";
                if (fgets(line, 255, fp) == NULL) {
                        continue;
                }
                //printf("parsing ipv4rule in line[%s]", line);
                char *s = line, *in[6], *sp=NULL;
                uint32_t num_cols = 5;
                static const char *dlm = ",";
                for (i = 0; i != num_cols; i++, s = NULL) {
                        in[i] = strtok_r(s, dlm, &sp);
                        if (in[i] == NULL) {
                                break;
                        }
                }
                if (i >=5) {
                        //onvm_ft_ipv4_5tuple read_tuple = {0,0,0,0,0};
                        //read_tuple.src_addr = ;
                        ipv4_5tRules[max_ft_entries].src_addr = get_ipv4_value(in[0]);
                        ipv4_5tRules[max_ft_entries].dst_addr = get_ipv4_value(in[1]);
                        ipv4_5tRules[max_ft_entries].src_port =(uint16_t)(strtoul(in[2],NULL,10));
                        ipv4_5tRules[max_ft_entries].dst_port =(uint16_t)(strtoul(in[3],NULL,10));
                        ipv4_5tRules[max_ft_entries].proto = (uint8_t) strtoul(in[4],NULL,10);
                        if( (ipv4_5tRules[max_ft_entries].src_addr && ipv4_5tRules[max_ft_entries].dst_addr) &&
                            (IP_PROTOCOL_TCP == ipv4_5tRules[max_ft_entries].proto  || IP_PROTOCOL_UDP == ipv4_5tRules[max_ft_entries].proto)) {
                                max_ft_entries++;
                        }
                }
                //if (MAX_FLOW_TABLE_ENTRIES <= max_ft_entries ) {
                if (globals.max_ft_rules <= max_ft_entries ) {
#ifdef DEBUG_0
                        printf("Reached the limit on allowed num of flow entries [%d]!", max_ft_entries);
#endif
                        break;
                }
        }
        if (fp) {
                fclose(fp);
        }
//#define DEBUG
#ifdef DEBUG
        printf("Read IPv4-5T List!\n");
        for(i=0; i< max_ft_entries; i++){
                printf("%u %u %u %u %u \n", ipv4_5tRules[i].src_addr, ipv4_5tRules[i].dst_addr,
                        ipv4_5tRules[i].src_port, ipv4_5tRules[i].dst_port, ipv4_5tRules[i].proto);
        }
        printf("\n\n************\n\n");
#else
        printf("Total FT Entries = [%d]", max_ft_entries);
#endif
        return;
}

/*******************************Helper functions********************************/
static int
setup_service_chain_for_flow_entry(struct onvm_service_chain *sc, int sc_index, int rev_order) {
        static uint32_t next_sc = 0;
        int index = 0, service_id=0, chain_len = 0;
        if (0 == max_service_chains) {
                return -1;
        }

        //Get the sc_index based on either valid passed value or static incremental
        //sc_index = (sc_index >= 0 && sc_index < max_service_chains)? (sc_index):((int)next_sc);
        uint8_t use_input_sc_index = 0;
        if(sc_index >= 0 && sc_index < max_service_chains) {
                use_input_sc_index = 1;
        }
        else {
                sc_index =  (int)next_sc;
        }

        /* Setup the chain in reverse order makes sense only when use_input_sc_index=1 */
        if (1 == use_input_sc_index && 1 == rev_order) {
                int last_index = 0;
                while(schains[sc_index][last_index++] != -1 || last_index < ONVM_MAX_CHAIN_LENGTH);
#ifdef DEBUG_0
                printf("\n Adding Flip Service chain of Length [%d]: ", last_index-1);
#endif
                for(index =last_index-1; index >=0; index--) {
                        service_id = schains[sc_index][index]; //service_id = schains[next_sc][index];
                        if (service_id > 0){
                               onvm_sc_append_entry(sc, ONVM_NF_ACTION_TONF, service_id);
#ifdef DEBUG_0
                                printf(" [%d] \t ", service_id);
#endif
                                chain_len++;
                        }
                }
        }
        else {
                for(index =0; index < ONVM_MAX_CHAIN_LENGTH; index++) {
                        service_id = schains[sc_index][index]; //service_id = schains[next_sc][index];
                        if (service_id > 0){
                                onvm_sc_append_entry(sc, ONVM_NF_ACTION_TONF, service_id);
#ifdef DEBUG_0
                                printf(" [%d] \t ", service_id);
#endif
                                chain_len++;
                        }
                }
        }
#ifdef DEBUG_0
        if(chain_len){
                printf("setup the service chain of length: %d\n ",chain_len);
                //next_sc = (next_sc+1)%max_service_chains;
        }
        else {
                printf("Didn't setup the service chain of length: %d\n ",chain_len);
        }
#endif
        if(!use_input_sc_index) {
                next_sc = (next_sc == ((uint32_t)(max_service_chains-1))?(0):(next_sc+1));
        }

        return sc_index; //chain_len
}
static int
setup_schain_and_flow_entry_for_flip_key(struct onvm_ft_ipv4_5tuple *fk_in, int sc_index, int rev_order);

static int
setup_schain_and_flow_entry_for_flip_key(struct onvm_ft_ipv4_5tuple *fk_in, int sc_index, int rev_order) {
        int ret = 0;
        if (NULL == fk_in) {
                ret = -1;
                return ret;
        }
        struct onvm_ft_ipv4_5tuple *fk = NULL;
        fk = rte_calloc("flow_key",1, sizeof(struct onvm_ft_ipv4_5tuple), 0); //RTE_CACHE_LINE_SIZE
        if (fk == NULL) {
#ifdef DEBUG_0
                printf("failed in rte_calloc \n");
#endif
                return -ENOMEM;
                rte_exit(EXIT_FAILURE, "Cannot allocate memory for flow key\n");
                exit(1);
        }
        //swap ip_addr info (Does it also need LE_to_BE_swap?? it should not)
        fk->src_addr = fk_in->dst_addr;
        fk->dst_addr = fk_in->src_addr;
        fk->src_port = fk_in->dst_port;
        fk->dst_port = fk_in->src_port;
        fk->proto    = fk_in->proto;
#ifdef DEBUG_0
        printf("\nAdding Flip rule for FlowKey [%x:%u:, %x:%u, %u]", fk->src_addr, fk->src_port, fk->dst_addr, fk->dst_port, fk->proto);
#endif
        struct onvm_flow_entry *flow_entry = NULL;
        ret = onvm_flow_dir_get_key(fk, &flow_entry);

        #ifdef DEBUG_0
        printf("\n Starting to assign sc for flow_entries with src_ip [%x] \n", fk->src_addr);
        #endif

        if (ret == -ENOENT) {
                flow_entry = NULL;
                ret = onvm_flow_dir_add_key(fk, &flow_entry);

                #ifdef DEBUG_0
                printf("Adding fresh Key [%x] for flow_entry\n", fk->src_addr);
                #endif
        }
        /* Entry already exists  */
        else if (ret >= 0) {
#ifdef DEBUG_0
                printf("Flow Entry in Table already exits at index [%d]\n\n", ret);
#endif
                return -EEXIST;
        }
        else {
#ifdef DEBUG_0
                printf("\n Existing due to unknown Failure in get_key()! \n");
#endif
                return ret;
                rte_exit(EXIT_FAILURE, "onvm_flow_dir_get parameters are invalid");
                exit(2);
        }

        onvm_flow_dir_reset_entry(flow_entry); //memset(flow_entry, 0, sizeof(struct onvm_flow_entry));
        flow_entry->key = fk;

        #ifdef DEBUG_0
        //printf("\n Enter in sc_create()! \n");
        #endif

        flow_entry->sc = onvm_sc_create();
        if (NULL ==  flow_entry->sc) {
                rte_exit(EXIT_FAILURE, "onvm_sc_create() Failed!!");
        }

        sc_index = setup_service_chain_for_flow_entry(flow_entry->sc, sc_index,rev_order);

        /* Setup the properties of Flow Entry */
        flow_entry->idle_timeout = 0; //OFP_FLOW_PERMANENT;
        flow_entry->hard_timeout = 0; //OFP_FLOW_PERMANENT;

        #ifdef DEBUG_0
        onvm_sc_print(flow_entry->sc);
        #endif

        return 0;

        return ret;
}

//#define DEBUG_0
static int setup_flow_rule_and_sc_entries(void);
static int
add_flow_key_to_sc_flow_table(struct onvm_ft_ipv4_5tuple *ft)
{
        int ret = 0;
        struct onvm_ft_ipv4_5tuple *fk = NULL;
        static struct onvm_flow_entry *flow_entry = NULL;

        if (NULL == ft){
                return -EINVAL;
        }

        fk = rte_calloc("flow_key",1, sizeof(struct onvm_ft_ipv4_5tuple), 0); //RTE_CACHE_LINE_SIZE
        if (fk == NULL) {
                printf("failed in rte_calloc \n");
                return -ENOMEM;
                rte_exit(EXIT_FAILURE, "Cannot allocate memory for flow key\n");
                exit(1);
        }

        #ifdef DEBUG_0
        printf("\n Adding New Rule.\n");
        #endif

        fk->proto = ft->proto;
        fk->src_addr = rte_cpu_to_be_32(ft->src_addr); //ft->src_addr;
        fk->src_port = rte_cpu_to_be_16(ft->src_port); //ft->src_port;
        fk->dst_addr = rte_cpu_to_be_32(ft->dst_addr); //ft->dst_addr;
        fk->dst_port = rte_cpu_to_be_16(ft->dst_port); //ft->dst_port;
#ifdef DEBUG_0
        printf("\nAdding [%x:%u:, %x:%u, %u]", fk->src_addr, fk->src_port, fk->dst_addr, fk->dst_port, fk->proto);
#endif

        ret = onvm_flow_dir_get_key(fk, &flow_entry);

        #ifdef DEBUG_0
        printf("\n Starting to assign sc for flow_entries with src_ip [%x] \n", fk->src_addr);
        #endif

        if (ret == -ENOENT) {
                flow_entry = NULL;
                ret = onvm_flow_dir_add_key(fk, &flow_entry);

                #ifdef DEBUG_0
                printf("Adding fresh Key [%x] for flow_entry\n", fk->src_addr);
                #endif
        }
        /* Entry already exists  */
        else if (ret >= 0) {
#ifdef DEBUG_0
                printf("Flow Entry in Table already exits at index [%d]\n\n", ret);
#endif
                return -EEXIST;
        }
        else {
#ifdef DEBUG_0
                printf("\n Existing due to unknown Failure in get_key()! \n");
#endif
                return ret;
                rte_exit(EXIT_FAILURE, "onvm_flow_dir_get parameters are invalid");
                exit(2);
        }

        onvm_flow_dir_reset_entry(flow_entry); //memset(flow_entry, 0, sizeof(struct onvm_flow_entry));
        flow_entry->key = fk;

        #ifdef DEBUG_0
        //printf("\n Enter in sc_create()! \n");
        #endif

        flow_entry->sc = onvm_sc_create();
        if (NULL ==  flow_entry->sc) {
                rte_exit(EXIT_FAILURE, "onvm_sc_create() Failed!!");
        }

        int sc_index = setup_service_chain_for_flow_entry(flow_entry->sc, -1,0);
        sc_index = setup_schain_and_flow_entry_for_flip_key(fk, sc_index,globals.rev_order);

        /* Setup the properties of Flow Entry */
        flow_entry->idle_timeout = 0; //OFP_FLOW_PERMANENT;
        flow_entry->hard_timeout = 0; //OFP_FLOW_PERMANENT;

        #ifdef DEBUG_0
        onvm_sc_print(flow_entry->sc);
        #endif

        return 0;
}
uint32_t populate_random_flow_rules(uint32_t max_rules);
uint32_t
populate_random_flow_rules(uint32_t max_rules) {
        uint32_t ret=0;

        /* Any better logic to generate the Table Entries?? */
        uint32_t base_ip = get_ipv4_value(globals.base_ip_addr);//must be input arg
        uint32_t max_ips = globals.max_ip_addrs;                //must be input arg
        uint16_t bs_port = 5001;                                //must be input arg
        uint16_t bd_port = 57304;                               //must be input arg
        uint32_t rules_added = max_ft_entries;

        /* Can make TCP/UDP as input arg and cut down on the rules: instead
         * setup bidirectional rules and replicate mirror of NF chain (reverse order)
         */
        while(rules_added < max_rules) {
                uint32_t sip_inc = 0;
                for (; sip_inc < max_ips; sip_inc++) {
                        uint32_t dip_inc = 0;
                        for(;dip_inc < max_ips; dip_inc++ ) {
                                #ifdef DEBUG_0
                                printf("\n Adding [TCP and UDP] Rules!");
                                #endif

                                //IP_PROTOCOL_UDP (6=TCP, 17=UDP, 1=ICMP)
                                ipv4_5tRules[rules_added].proto = IP_PROTOCOL_TCP;
                                ipv4_5tRules[rules_added].src_addr = (base_ip + sip_inc);
                                ipv4_5tRules[rules_added].src_port = bs_port;
                                ipv4_5tRules[rules_added].dst_addr = (base_ip + (sip_inc+1+dip_inc)%max_ips);
                                ipv4_5tRules[rules_added].dst_port = bd_port;

#ifdef DEBUG_0
                                struct onvm_ft_ipv4_5tuple *fk = &ipv4_5tRules[rules_added];
                                printf("\nAdding to ipv4FT [TCP, %x:%x:, %x:%x]", fk->src_addr,
                                                fk->src_port, fk->dst_addr, fk->dst_port);
                                #endif
                                rules_added++;
                                if(rules_added >= max_rules) break;

                                //IP_PROTOCOL_UDP (6=TCP, 17=UDP, 1=ICMP)
                                ipv4_5tRules[rules_added].proto = IP_PROTOCOL_UDP;
                                ipv4_5tRules[rules_added].src_addr = (base_ip + sip_inc);
                                ipv4_5tRules[rules_added].src_port = bs_port;
                                ipv4_5tRules[rules_added].dst_addr = (base_ip + (sip_inc+1+dip_inc)%max_ips);
                                ipv4_5tRules[rules_added].dst_port = bd_port;
                                #ifdef DEBUG_0
                                fk = &ipv4_5tRules[rules_added];
                                printf("\nAdding to ipv4FT [UDP, %x:%x:, %x:%x]", fk->src_addr,
                                                fk->src_port, fk->dst_addr, fk->dst_port);
                                #endif
                                rules_added++;
                                if(rules_added >= max_rules) break;
                        }
                }
                if(rules_added >= max_rules) break;
                bs_port+=10;
                bd_port+=10;
        }
        ret = (rules_added - max_ft_entries);

        //#ifdef DEBUG_0
        printf("Total random_rules_added: %u", ret);
        //#endif
        return ret;
}

static int
setup_flow_rule_and_sc_entries(void) {

        int ret = 0;
        uint32_t random_flows = 0; //(MIN(globals.max_ft_rules/2,MAX_FLOW_TABLE_ENTRIES));   populate_random_flow_rules
        max_ft_entries+= random_flows;

        /* Now add the Flow Tuples to the Global Flow Table with appropriate Service chains */
        uint32_t i = 0;
        for (;i < max_ft_entries;i++) {
                ret = add_flow_key_to_sc_flow_table(&ipv4_5tRules[i]);
        }
#ifdef DEBUG_0
        printf("\n\n Populated %d flow table rules with service chains!\n", max_ft_entries);
#endif
        return ret;
}

static int setup_flowrule_for_packet(struct rte_mbuf *pkt, struct onvm_pkt_meta* meta) {

        int ret = 0;
        uint8_t ipv4_pkt = 0;

        struct onvm_flow_entry *flow_entry = NULL;
        struct onvm_ft_ipv4_5tuple fk;

        /* Unknown packet type */
        if (pkt->hash.rss == 0) {
                printf("Setting to redirect on alternate port\n ");
                if(ports->num_ports > 1) meta->destination = (pkt->port == 0)? (1):(0);
                else meta->destination = pkt->port;
                meta->action = ONVM_NF_ACTION_OUT; //ONVM_NF_ACTION_DROP;
                //onvm_pkt_print(pkt);
                return 0;
        }
        meta->src = 0;
        meta->chain_index = 0;

        /* Check if it is IPv4 packet and get Ipv4 tuple Key*/
        if (!onvm_ft_fill_key(&fk, pkt)) {
                ipv4_pkt = 1;
        } else { // Not and Ipv4 pkt
#ifdef DEBUG_0
                printf("No IP4 header found\n");
                onvm_pkt_print(pkt);
#endif
        }

        /* Get the Flow Entry for this packet:: Must fail if there is no entry in flow_table */
        ret = onvm_flow_dir_get_pkt(pkt, &flow_entry);
        // Success case: Duplicate packet requesting entry, make it point to first index and return to make pkt proceed with already setup chain
        if (ret >= 0 && flow_entry != NULL) {
                #ifdef DEBUG_0
                printf("Exisitng_S:[%x] \n", pkt->hash.rss);
                #endif
                meta->action = flow_entry->sc->sc[meta->chain_index].action;            //ONVM_NF_ACTION_NEXT;
                meta->destination = flow_entry->sc->sc[meta->chain_index].destination;  //globals.destination;
                meta->chain_index -=1; //  (meta->chain_index)--;
                #ifdef DEBUG_0
                printf("Exisitng_E:\n"); //onvm_sc_print(flow_entry->sc);
                #endif
        }
        // Expected Failure case: setup the Flow table entry with appropriate service chain
        else {
                #ifdef DEBUG_0
                printf("Setting new_S for [%x]:\n", pkt->hash.rss);
                #endif
                ret = onvm_flow_dir_add_pkt(pkt, &flow_entry);
                if (NULL == flow_entry) {
#ifdef DEBUG_0
                        printf("Couldn't get the flow entry!!");
#endif
                        return 0;
                }
                onvm_flow_dir_reset_entry(flow_entry); //memset(flow_entry, 0, sizeof(struct onvm_flow_entry));
                flow_entry->sc = onvm_sc_create();
                int sc_index = setup_service_chain_for_flow_entry(flow_entry->sc, -1,0);

                if(ipv4_pkt) {
                        //set the same schain for flow_entry with flipped ipv4 % Tuple rule
                        sc_index = setup_schain_and_flow_entry_for_flip_key(&fk, sc_index,globals.rev_order);
                } else {
#ifdef DEBUG_0
                        printf("Skipped adding Flip rule \n");
#endif
                }

                //onvm_sc_append_entry(flow_entry->sc, ONVM_NF_ACTION_TONF, globals.destination);
                #ifdef DEBUG_0
                printf("Setting new_E:\n");//onvm_sc_print(flow_entry->sc);
                #endif
                meta->action = ONVM_NF_ACTION_NEXT;//ONVM_NF_ACTION_NEXT; ONVM_NF_ACTION_DROP;
        }
               return 0;
}

/*******************************File Interface functions********************************/
int init_onvm_ft_install(void) {
        memset(schains, -1, sizeof(int) * ONVM_MAX_CHAIN_LENGTH*MAX_SERVICE_CHAINS );

        //return 0;
        /* Get service chain types */
        parse_service_chains(schains);

        /* Get IP-5Tuple entries to pre install Flow rules */
        parse_ipv4_5t_rules();

        /* Setup pre-defined set of FT entries UDP protocol: Extend optargs as necessary */
        setup_flow_rule_and_sc_entries();

        return 0;
}

int onvm_ft_handle_packet(struct rte_mbuf *pkt, struct onvm_pkt_meta* meta) {

        return setup_flowrule_for_packet(pkt,meta);
}
