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
 * firewall.c - firewall implementation using ONVM
 ********************************************************************/

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <libgen.h>
#include <inttypes.h>
#include <limits.h>
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

#define MAX_RULES 256
#define NUM_TBLS 8

static uint16_t destination;
static int debug = 0;
char *rule_file = NULL;

/* Structs that contain information to setup LPM and its rules */
struct lpm_request *firewall_req;
static struct firewall_pkt_stats stats;
struct rte_lpm *lpm_tbl;
struct onvm_fw_rule **rules;

/* Number of packets between each print */
static uint32_t print_delay = 10000000;

/* Shared data structure containing host port info */
extern struct port_info *ports;

/* Struct for the firewall LPM rules */
struct onvm_fw_rule {
        uint32_t src_ip;
        uint8_t depth;
        uint8_t action;
};

/* Struct for printing stats */
struct firewall_pkt_stats {
        uint64_t pkt_drop;
        uint64_t pkt_accept;
        uint64_t pkt_not_ipv4;
        uint64_t pkt_total;
};

/*
 * Print a usage message
 */
static void
usage(const char *progname) {
        printf("Usage: %s [EAL args] -- [NF_LIB args] -- -p <print_delay> -f <rules file> [-b]\n\n", progname);
        printf("Flags:\n");
        printf(" - `-d DST`: Destination Service ID to forward to\n");
        printf(" - `-p PRINT_DELAY`: Number of packets between each print, e.g. `-p 1` prints every packets.\n");
        printf(" - `-b`: Debug mode: Print each incoming packets source/destination"
               " IP address as well as its drop/forward status\n");
        printf(" - `-f`: Path to a JSON file containing firewall rules; See README for example usage\n");
}

/*
 * Parse the application arguments.
 */
static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c, dst_flag = 0, rules_init = 0;

        while ((c = getopt(argc, argv, "d:f:p:b")) != -1) {
                switch (c) {
                        case 'd':
                                destination = strtoul(optarg, NULL, 10);
                                dst_flag = 1;
                                break;
                        case 'p':
                                print_delay = strtoul(optarg, NULL, 10);
                                RTE_LOG(INFO, APP, "Print delay = %d\n", print_delay);
                                break;
                        case 'f':
                                rule_file = strdup(optarg);
                                rules_init = 1;
                                break;
                        case 'b':
                                RTE_LOG(INFO, APP, "Debug mode enabled; printing the source IP addresses"
                                                   " of each incoming packet as well as drop/forward status\n");
                                debug = 1;
                                break;
                        case '?':
                                usage(progname);
                                if (optopt == 'p')
                                        RTE_LOG(INFO, APP, "Option -%c requires an argument.\n", optopt);
                                if (optopt == 'd')
                                        RTE_LOG(INFO, APP, "Option -%c requires an argument.\n", optopt);
                                if (optopt == 'f')
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

        if (!dst_flag) {
                RTE_LOG(INFO, APP, "Firewall NF requires a destination NF with the -d flag.\n");
                return -1;
        }
        if (!debug) {
                RTE_LOG(INFO, APP, "Running normal mode, use -b flag to enable debug mode\n");
        }
        if (!rules_init) {
                RTE_LOG(INFO, APP, "Please specify a rules JSON file with -f FILE_NAME\n");
                return -1;
        }
        return optind;
}

/*
 * This function displays stats. It uses ANSI terminal codes to clear
 * screen when called. It is called from a single non-master
 * thread in the server process, when the process is run with more
 * than one lcore enabled.
 */
static void
do_stats_display(void) {
        const char clr[] = {27, '[', '2', 'J', '\0'};
        const char topLeft[] = {27, '[', '1', ';', '1', 'H', '\0'};

        /* Clear screen and move to top left */
        printf("%s%s", clr, topLeft);
        printf("Packets Dropped: %lu\n", stats.pkt_drop);
        printf("Packets not IPv4: %lu\n", stats.pkt_not_ipv4);
        printf("Packets Accepted: %lu\n", stats.pkt_accept);
        printf("Packets Total: %lu", stats.pkt_total);

        printf("\n\n");
}

static int
packet_handler(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta,
               __attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx) {
        struct ipv4_hdr *ipv4_hdr;
        static uint32_t counter = 0;
        int ret;
        uint32_t rule = 0;
        uint32_t track_ip = 0;
        char ip_string[16];

        if (++counter == print_delay) {
                do_stats_display();
                counter = 0;
        }

        stats.pkt_total++;

        if (!onvm_pkt_is_ipv4(pkt)) {
                if (debug) RTE_LOG(INFO, APP, "Packet received not ipv4\n");
                stats.pkt_not_ipv4++;
                meta->action = ONVM_NF_ACTION_DROP;
                return 0;
        }

        ipv4_hdr = onvm_pkt_ipv4_hdr(pkt);
        ret = rte_lpm_lookup(lpm_tbl, rte_be_to_cpu_32(ipv4_hdr->src_addr), &rule);

        if (debug) onvm_pkt_parse_char_ip(ip_string, rte_be_to_cpu_32(ipv4_hdr->src_addr));

        if (ret < 0) {
                meta->action = ONVM_NF_ACTION_DROP;
                stats.pkt_drop++;
                if (debug) RTE_LOG(INFO, APP, "Packet from source IP %s has been dropped\n", ip_string);
                return 0;
        }

        switch (rule) {
                case 0:
                        meta->action = ONVM_NF_ACTION_TONF;
                        meta->destination = destination;
                        stats.pkt_accept++;
                        if (debug) RTE_LOG(INFO, APP, "Packet from source IP %s has been accepted\n", ip_string);
                        break;
                default:
                        meta->action = ONVM_NF_ACTION_DROP;
                        stats.pkt_drop++;
                        if (debug) RTE_LOG(INFO, APP, "Packet from source IP %s has been dropped\n", ip_string);
                        break;
        }

        return 0;
}

static int
lpm_setup(struct onvm_fw_rule **rules, int num_rules) {
        int i, status, ret;
        uint32_t ip;
        char name[64];
        char ip_string[16];

        firewall_req = (struct lpm_request *) rte_malloc(NULL, sizeof(struct lpm_request), 0);

        if (!firewall_req) return 0;

        snprintf(name, sizeof(name), "fw%d-%"PRIu64, rte_lcore_id(), rte_get_tsc_cycles());
        firewall_req->max_num_rules = 1024;
        firewall_req->num_tbl8s = 24;
        firewall_req->socket_id = rte_socket_id();
        snprintf(firewall_req->name, sizeof(name), "%s", name);
        status = onvm_nflib_request_lpm(firewall_req);

        if (status < 0) {
                rte_exit(EXIT_FAILURE, "Cannot get lpm region for firewall\n");
        }

        lpm_tbl = rte_lpm_find_existing(name);

        if (lpm_tbl == NULL) {
                printf("No existing LPM_TBL\n");
        }

        for (i = 0; i < num_rules; ++i) {
                ip = rules[i]->src_ip;
                onvm_pkt_parse_char_ip(ip_string, ip);
                printf("RULE %d: { ip: %s, depth: %d, action: %d }\n", i, ip_string, rules[i]->depth, rules[i]->action);
                ret = rte_lpm_add(lpm_tbl, rules[i]->src_ip, rules[i]->depth, rules[i]->action);
                if (ret < 0) {
                        printf("ERROR ADDING RULE %d\n", ret);
                        return 1;
                }
        }
        rte_free(firewall_req);

        return 0;
}

static void
lpm_teardown(struct onvm_fw_rule **rules, int num_rules) {
        int i;

        if (rules) {
                for (i = 0; i < num_rules; ++i) {
                        if (rules[i]) free(rules[i]);
                }
                free(rules);
        }

        if (lpm_tbl) {
                rte_lpm_free(lpm_tbl);
        }

        if (rule_file) {
                free(rule_file);
        }
}

struct onvm_fw_rule
**setup_rules(int *total_rules, char *rules_file) {
        int ip[4];
        int num_rules, ret;
        int i = 0;

        cJSON *rules_json = onvm_config_parse_file(rules_file);
        cJSON *rules_ip = NULL;
        cJSON *depth = NULL;
        cJSON *action = NULL;

        if (rules_json == NULL) {
                rte_exit(EXIT_FAILURE, "%s file could not be parsed/not found. Assure rules file"
                                       " the directory to the rules file is being specified.\n", rules_file);
        }

        num_rules = onvm_config_get_item_count(rules_json);
        *total_rules = num_rules;
        rules = (struct onvm_fw_rule **) malloc(num_rules * sizeof(struct onvm_fw_rule *));
        rules_json = rules_json->child;

        while (rules_json != NULL) {
                rules_ip = cJSON_GetObjectItem(rules_json, "ip");
                depth = cJSON_GetObjectItem(rules_json, "depth");
                action = cJSON_GetObjectItem(rules_json, "action");

                if (rules_ip == NULL) rte_exit(EXIT_FAILURE, "IP not found/invalid\n");
                if (depth == NULL) rte_exit(EXIT_FAILURE, "Depth not found/invalid\n");
                if (action == NULL) rte_exit(EXIT_FAILURE, "Action not found/invalid\n");

                rules[i] = (struct onvm_fw_rule *) malloc(sizeof(struct onvm_fw_rule));
                onvm_pkt_parse_ip(rules_ip->valuestring, &rules[i]->src_ip);
                rules[i]->depth = depth->valueint;
                rules[i]->action = action->valueint;
                rules_json = rules_json->next;
                i++;
        }
        cJSON_Delete(rules_json);

        return rules;
}

int main(int argc, char *argv[]) {
        struct onvm_nf_local_ctx *nf_local_ctx;
        struct onvm_nf_function_table *nf_function_table;
        struct onvm_fw_rule **rules;
        int arg_offset, num_rules;

        const char *progname = argv[0];
        stats.pkt_drop = 0;
        stats.pkt_accept = 0;

        nf_local_ctx = onvm_nflib_init_nf_local_ctx();
        onvm_nflib_start_signal_handler(nf_local_ctx, NULL);

        nf_function_table = onvm_nflib_init_nf_function_table();
        nf_function_table->pkt_handler = &packet_handler;

        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG, nf_local_ctx, nf_function_table)) < 0) {
                onvm_nflib_stop(nf_local_ctx);
                if (arg_offset == ONVM_SIGNAL_TERMINATION) {
                        printf("Exiting due to user termination\n");
                        return 0;
                } else {
                        rte_exit(EXIT_FAILURE, "Failed ONVM init\n");
                }
        }

        argc -= arg_offset;
        argv += arg_offset;

        if (parse_app_args(argc, argv, progname) < 0) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        }

        rules = setup_rules(&num_rules, rule_file);
        lpm_setup(rules, num_rules);
        onvm_nflib_run(nf_local_ctx);

        lpm_teardown(rules, num_rules);
        onvm_nflib_stop(nf_local_ctx);
        printf("If we reach here, program is ending\n");
        return 0;
}
