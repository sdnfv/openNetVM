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
 * firewall.c - firewall implementation using ONVM
 ********************************************************************/

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include "cJSON.h"

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_malloc.h>

#include <rte_lpm.h>

#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"
#include "onvm_config_common.h"

#define NF_TAG "firewall"

#define FW_ACTION_ACCEPT 1

#define MAX_RULES 256
#define NUM_TBLS 8

static uint16_t destination;

/* Struct that contains information about this NF */
struct onvm_nf_info *nf_info;

/* shared data structure containing host port info */
extern struct port_info *ports;

// struct for parsing rules
struct onvm_fw_rule {
        uint32_t src_ip;
        uint8_t depth;
        uint8_t action;
};

/*
 * Print a usage message
 */
static void
usage(const char *progname) {
        printf("Usage: %s [EAL args] -- [NF_LIB args] -- -p <print_delay>\n\n", progname);
}

/*
 * Parse the application arguments.
 */
static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c;

        while ((c = getopt (argc, argv, "d:p:")) != -1) {
                switch (c) {
                case 'd':
                        destination = strtoul(optarg, NULL, 10);
                        break;
                case 'p':
                        RTE_LOG(INFO, APP, "print_delay = %d\n", 0);
                        break;
                case '?':
                        usage(progname);
                        if (optopt == 'p')
                                RTE_LOG(INFO, APP, "Option -%c requires an argument.\n", optopt);
                        else if (isprint(optopt))
                                RTE_LOG(INFO, APP, "Unknown option `-%c'.\n", optopt);
                        else
                                RTE_LOG(INFO, APP, "Unknown option character `\\x%x'.\n", optopt);
                        return -1;
                default:
                        usage(progname);
                        return -1;
                }
        }
        return optind;
}

struct rte_lpm* lpm_tbl;
struct lpm_request* req;

static int
packet_handler(struct rte_mbuf* pkt, struct onvm_pkt_meta* meta, struct onvm_nf_info* nf_info) {
        struct ipv4_hdr* ipv4_hdr;
        uint32_t rule = 0;
        uint32_t track_ip = 0;

        if(onvm_pkt_is_ipv4(pkt)){
                ipv4_hdr = onvm_pkt_ipv4_hdr(pkt);

                int no_result = rte_lpm_lookup(lpm_tbl, ipv4_hdr->src_addr, &rule);
                if(!no_result){
                        switch(rule){
                        case ONVM_NF_ACTION_TONF:
                                meta->action = ONVM_NF_ACTION_TONF;
                                meta->destination = destination;
                                //printf("Dest: %d\n", destination);
                                RTE_LOG(INFO, APP, "Packet from IP %d has been accepted.\n", ipv4_hdr->src_addr);
                                break;
                        default:
                                // if we can't understand the rule, drop it
                                meta->action = ONVM_NF_ACTION_DROP;
                                RTE_LOG(INFO, APP, "Packet from IP %d has been dropped.\n", ipv4_hdr->src_addr);
                                break;
                        }
                } else {
                        // no matching rule
                        // default action is to drop packets
                        meta->action = ONVM_NF_ACTION_DROP;
                        RTE_LOG(INFO, APP, "Packet from IP %d has been dropped.\n", ipv4_hdr->src_addr);
                }
        } else {
                // drop all packets that aren't ipv4
                RTE_LOG(INFO, APP, "Packet received not ipv4\n");
                meta->action = ONVM_NF_ACTION_DROP;
        }

        return 0;
}

static int lpm_setup(struct onvm_fw_rule** rules, int num_rules) {
        int i, status;

        req = (struct lpm_request*)rte_malloc(NULL, sizeof(struct lpm_request), 0);

        if(!req) return 0;

        req->max_num_rules = 1024;
        req->num_tbl8s = 24;
        req->socket_id = rte_socket_id();
        req->name[0] = 'f';
        req->name[1] = 'w';

        status = onvm_nflib_request_lpm(req);

	    if(status < 0){
		        rte_exit(EXIT_FAILURE, "Cannot get lpm region for firewall");
	    }

        lpm_tbl = rte_lpm_find_existing("fw");

	    if (lpm_tbl == NULL) {
	            printf("No existing LPM_TBL\n");
	    }

        for(i = 0; i < num_rules; ++i){
                printf("RULE { ip: %d, depth: %d, action: %d }\n", rules[i]->src_ip, rules[i]->depth, rules[i]->action);
                int add_failed = rte_lpm_add(lpm_tbl, rules[i]->src_ip, rules[i]->depth, rules[i]->action);
                printf("Src_ip = %d depth = %d action = %d ", rules[i]->src_ip, rules[i]->depth, rules[i]->action);
                if(add_failed){
                        printf("ERROR ADDING RULE %d\n", add_failed);
                        return 1;
                }
        }

        return 0;
}

static void lpm_teardown(struct onvm_fw_rule** rules, int num_rules){
        int i;

        if(rules){
                for(i = 0; i < num_rules; ++i){
                        if(rules[i]) free(rules[i]);
                }

                free(rules);
        }
}


int main(int argc, char *argv[]) {
        int arg_offset;
        struct onvm_fw_rule** rules;

        const char *progname = argv[0];

        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG, &nf_info)) < 0)
                return -1;
        argc -= arg_offset;
        argv += arg_offset;

        if (parse_app_args(argc, argv, progname) < 0) {
                onvm_nflib_stop(nf_info);
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        }

        cJSON *rules_json = onvm_config_parse_file("rules.json");
        cJSON *rules_name = NULL;
        char *rule_num = NULL;

        if (rules_json == NULL) {
                rte_exit(EXIT_FAILURE, "Rules.json file could not be parsed\n");
        }
        else {
                RTE_LOG(INFO, APP, "Rules.json parsed\n");
        }

        rules_name = cJSON_GetObjectItem(rules_json, "name");
        rule_num = cJSON_GetArrayItem(rules_name, 0)->valuestring;
        //printf("%d", rule_num);
        int num_rules = cJSON_GetArraySize(rules_name);
        //RTE_LOG(INFO, APP, "Rules.json name: %s\n", rule_num);

//        if (rules_name->valuestring != NULL) {
//                RTE_LOG(INFO, APP, "Rules.json name: %s\n", rules_name->valuestring);
//        }



        rules = (struct onvm_fw_rule**)malloc(1 * sizeof(struct onvm_fw_rule*));
        rules[0] = (struct onvm_fw_rule*)malloc(sizeof(struct onvm_fw_rule));
        rules[0]->src_ip = 110019;
        rules[0]->depth = 32;
        rules[0]->action = ONVM_NF_ACTION_TONF;
        printf("Num rules: %d\n", num_rules);

        lpm_setup(rules, num_rules);



        onvm_nflib_run(nf_info, &packet_handler);
        free(req->name);
        free(req);
        lpm_teardown(rules, num_rules);
        printf("If we reach here, program is ending");
        return 0;
}
