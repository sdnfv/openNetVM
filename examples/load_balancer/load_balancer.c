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
 *  load_balancer.c - an example Layer 3 round-robin load balancer.
 ********************************************************************/

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_memory.h>
#include <rte_memzone.h>

#include "onvm_flow_table.h"
#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"
#include "onvm_config_common.h"


#include "cJSON.h"

#define NF_TAG "load_balancer"
#define TABLE_SIZE 65536

/* Struct for load balancer information */
struct loadbalance {
        struct onvm_ft *ft;

        /* backend server information */
        uint8_t server_count;
        struct backend_server *server;

        /* for cleaning up connections */
        uint16_t num_stored;
        uint64_t elapsed_cycles;
        uint64_t last_cycles;
        uint32_t expire_time;

        /* port and ip values */
        uint32_t ip_lb_server;
        uint32_t ip_lb_client;
        uint8_t server_port;
        uint8_t client_port;

        /* config file */
        char *cfg_filename;
        
        /* LB policy */
        char *policy;

        /* structures to store server weights */
        int *weights;
        int total_weight;
};

/* Struct for backend servers */
struct backend_server {
        uint8_t d_addr_bytes[RTE_ETHER_ADDR_LEN];
        uint32_t d_ip;
};

/* Struct for flow info */
struct flow_info {
        uint8_t dest;
        uint8_t s_addr_bytes[RTE_ETHER_ADDR_LEN];
        uint64_t last_pkt_cycles;
        int is_active;
};

struct loadbalance *lb;
/* number of package between each print */
static uint32_t print_delay = 1000000;

/* onvm struct for port info lookup */
extern struct port_info *ports;

/*
 * Print a usage message
 */
static void
usage(const char *progname) {
        printf("Usage:\n");
        printf(
            "%s [EAL args] -- [NF_LIB args] -- [ -c client_iface] [-s server_iface] [-f server_config] -p "
            "<print_delay>\n",
            progname);
        printf("%s -F <CONFIG_FILE.json> [EAL args] -- [NF_LIB args] -- [NF args]\n\n", progname);
        printf("Flags:\n");
        printf(" - `-c CLIENT_IP` : client IP address\n");
        printf(" - `-r CLIENT_PORT` : client port ID\n");
        printf(" - `-s SERVER_IP` : server IP address\n");
        printf(" - `-t SERVER_PORT` : server port ID\n");
        printf(" - `-f SERVER_CONFIG` : backend server config file\n");
        printf(" - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.\n");
}

/*
 * Parse application IP-addresses for LB interfaces.
 */
static int
parse_iface_ip(char *arg, uint32_t *ip_dest) {
        int ret;

        ret = onvm_pkt_parse_ip(arg, ip_dest);
        if (ret < 0) {
                return -1;
        }
        *ip_dest = rte_cpu_to_be_32(*ip_dest);
        return 0;
}

/*
 * Parse the application arguments.
 */
static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c, ret;

        lb->cfg_filename = NULL;
        lb->client_port = RTE_MAX_ETHPORTS;
        lb->server_port = RTE_MAX_ETHPORTS;

        while ((c = getopt(argc, argv, "c:r:s:t:f:p:")) != -1) {
                switch (c) {
                        case 'c':
                                ret = parse_iface_ip(strdup(optarg), &lb->ip_lb_client);
                                if (ret < 0) {
                                        RTE_LOG(INFO, APP, "Error parsing client IP address");
                                        return -1;
                                }
                                break;
                        case 'r':
                                lb->client_port = atoi(strdup(optarg));
                                break;
                        case 's':
                                ret = parse_iface_ip(strdup(optarg), &lb->ip_lb_server);
                                if (ret < 0) {
                                        RTE_LOG(INFO, APP, "Error parsing server IP address");
                                        return -1;
                                }
                                break;
                        case 't':
                                lb->server_port = atoi(strdup(optarg));
                                break;
                        case 'f':
                                lb->cfg_filename = strdup(optarg);
                                break;
                        case 'p':
                                print_delay = strtoul(optarg, NULL, 10);
                                break;
                        case '?':
                                usage(progname);
                                if (optopt == 'd')
                                        RTE_LOG(INFO, APP, "Option -%c requires an argument.\n", optopt);
                                else if (optopt == 'p')
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

        if (!lb->cfg_filename) {
                RTE_LOG(INFO, APP, "Load balancer NF requires a backend server config file.\n");
                return -1;
        }

        if (lb->client_port == RTE_MAX_ETHPORTS) {
                RTE_LOG(INFO, APP, "Load balancer NF requires a client port ID.\n");
                return -1;
        }

        if (!lb->ip_lb_client) {
                RTE_LOG(INFO, APP, "Load balancer NF requires a client IP address.\n");
                return -1;
        }

        if (lb->server_port == RTE_MAX_ETHPORTS) {
                RTE_LOG(INFO, APP, "Load balancer NF requires a server port ID.\n");
                return -1;
        }

        if (!lb->ip_lb_server) {
                RTE_LOG(INFO, APP, "Load balancer NF requires a server IP address.\n");
                return -1;
        }

        return optind;
}

/*
 * This function parses the backend config. It takes the filename
 * and fills up the backend_server array. This includes the mac and ip
 * address of the backend servers as well as their weights
 */
static int
parse_backend_json_config(void) {
        int ret, i;
        i = 0;

        cJSON *config_json = onvm_config_parse_file(lb->cfg_filename);
        cJSON *list_size = NULL;
        cJSON *policy = NULL;
        cJSON *ip_addr = NULL;
        cJSON *mac_addr = NULL;
        cJSON *weight = NULL;

        if (config_json == NULL) {
                rte_exit(EXIT_FAILURE, "%s file could not be parsed/not found. Assure config file"
                                       " the directory to the config file is being specified.\n", lb->cfg_filename);
        }

        config_json = config_json -> child;

        list_size = cJSON_GetObjectItem(config_json, "LIST_SIZE");
        policy = cJSON_GetObjectItem(config_json, "policy");
        
        if (list_size == NULL) rte_exit(EXIT_FAILURE, "LIST_SIZE not found/invalid\n");
        if (policy == NULL) rte_exit(EXIT_FAILURE, "Policy not found/invalid\n");

        lb->server_count = list_size->valueint;
        lb->policy = strdup(policy->valuestring);

        if (!((!strcmp(lb->policy,"random")) || (!strcmp(lb->policy,"rrobin")) || (!strcmp(lb->policy,"weighted_random")))) {
                rte_exit(EXIT_FAILURE, "Invalid policy. Check server.conf\n");
        }
        
        
        lb->weights = (int*)calloc(lb->server_count,sizeof(int));

        lb->server = (struct backend_server *)rte_malloc("backend server info",
                                                         sizeof(struct backend_server) * lb->server_count, 0);
        if (lb->server == NULL) {
                rte_exit(EXIT_FAILURE, "Malloc failed, can't allocate server information\n");
        }

        config_json = config_json->next;


        while (config_json != NULL) {
                ip_addr = cJSON_GetObjectItem(config_json, "ip");
                mac_addr =  cJSON_GetObjectItem(config_json, "mac_addr");
                weight = cJSON_GetObjectItem(config_json, "weight");

                if (ip_addr == NULL) rte_exit(EXIT_FAILURE, "IP not found/invalid\n");
                if (mac_addr == NULL) rte_exit(EXIT_FAILURE, "MAC address not found/invalid\n");
                if (weight == NULL) rte_exit(EXIT_FAILURE, "Weight not found/invalid\n");

                ret = onvm_pkt_parse_ip(ip_addr->valuestring, &lb->server[i].d_ip);
                if (ret < 0) {
                        rte_exit(EXIT_FAILURE, "Error parsing config IP address #%d\n", i);
                }
                ret = onvm_pkt_parse_mac(mac_addr->valuestring, lb->server[i].d_addr_bytes);
                if (ret < 0) {
                        rte_exit(EXIT_FAILURE, "Error parsing config MAC address #%d\n", i);
                }

                if (strcmp(lb->policy, "weighted_random")) lb->weights[i] = 1;
                else {
                        lb->weights[i] = weight->valueint;
                        lb->total_weight += weight->valueint;
                }
                config_json = config_json->next;
                i++;

                

        }
        cJSON_Delete(config_json);

        printf("\nARP config:\n");
        for (i = 0; i < lb->server_count; i++) {
                printf("%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8 " ", (lb->server[i].d_ip >> 24) & 0xFF,
                       (lb->server[i].d_ip >> 16) & 0xFF, (lb->server[i].d_ip >> 8) & 0xFF,
                       lb->server[i].d_ip & 0xFF);
                printf("%02x:%02x:%02x:%02x:%02x:%02x\n", lb->server[i].d_addr_bytes[0], lb->server[i].d_addr_bytes[1],
                       lb->server[i].d_addr_bytes[2], lb->server[i].d_addr_bytes[3], lb->server[i].d_addr_bytes[4],
                       lb->server[i].d_addr_bytes[5]);
        }

        return ret;     
}

/*
 * This function displays stats. It uses ANSI terminal codes to clear
 * screen when called. It is called from a single non-master
 * thread in the server process, when the process is run with more
 * than one lcore enabled.
 */
static void
do_stats_display(struct rte_mbuf *pkt) {
        const char clr[] = {27, '[', '2', 'J', '\0'};
        const char topLeft[] = {27, '[', '1', ';', '1', 'H', '\0'};
        static uint64_t pkt_process = 0;
        struct rte_ipv4_hdr *ip;

        pkt_process += print_delay;

        /* Clear screen and move to top left */
        printf("%s%s", clr, topLeft);

        printf("PACKETS\n");
        printf("-----\n");
        printf("Port : %d\n", pkt->port);
        printf("Size : %d\n", pkt->pkt_len);
        printf("N°   : %" PRIu64 "\n", pkt_process);
        printf("\n\n");

        ip = onvm_pkt_ipv4_hdr(pkt);
        if (ip != NULL) {
                onvm_pkt_print(pkt);
        } else {
                printf("No IP4 header found\n");
        }
}

/*
 * Print flow information
 */
static void
print_flow_info(struct flow_info *f) {
        printf("Flow INFO\n");
        printf("Destination server: %d\n", f->dest);
        printf("Source mac %02x:%02x:%02x:%02x:%02x:%02x\n", f->s_addr_bytes[0], f->s_addr_bytes[1], f->s_addr_bytes[2],
               f->s_addr_bytes[3], f->s_addr_bytes[4], f->s_addr_bytes[5]);
}

/*
 * Parse and print load balancer server/client interface information, check port validity
 */
static void
validate_iface_config(void) {
        int i;
        int client_flag = 0;
        int server_flag = 0;
        uint8_t client_addr_bytes[RTE_ETHER_ADDR_LEN];
        uint8_t server_addr_bytes[RTE_ETHER_ADDR_LEN];
        

        /* Compare the interfaces to onvm_mgr ports by hwaddr and assign port id accordingly */
        for (i = 0; i < ports->num_ports; i++) {
                if (lb->client_port == ports->id[i]) {
                        memcpy(client_addr_bytes, &ports->mac[i], RTE_ETHER_ADDR_LEN);
                        client_flag = 1;
                }
                if (lb->server_port == ports->id[i]) {
                        memcpy(server_addr_bytes, &ports->mac[i], RTE_ETHER_ADDR_LEN);
                        server_flag = 1;
                }
        }

        if (!client_flag) {
                rte_exit(EXIT_FAILURE, "Client port ID invalid.\n");
        }

        if (!server_flag) {
                rte_exit(EXIT_FAILURE, "Server port ID invalid.\n");
        }

        printf("\nLoad balancer interfaces:\n");
        printf("Client iface: ID: %d, IP: %" PRIu32 " (%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8 "), ",
               lb->client_port, lb->ip_lb_client, lb->ip_lb_client & 0xFF,
               (lb->ip_lb_client >> 8) & 0xFF, (lb->ip_lb_client >> 16) & 0xFF, (lb->ip_lb_client >> 24) & 0xFF);
        printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", client_addr_bytes[0], client_addr_bytes[1], client_addr_bytes[2],
               client_addr_bytes[3], client_addr_bytes[4], client_addr_bytes[5]);
        printf("Server iface: ID: %d, IP: %" PRIu32 " (%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8 "), ",
               lb->server_port, lb->ip_lb_server, lb->ip_lb_server & 0xFF,
               (lb->ip_lb_server >> 8) & 0xFF, (lb->ip_lb_server >> 16) & 0xFF, (lb->ip_lb_server >> 24) & 0xFF);
        printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", server_addr_bytes[0], server_addr_bytes[1], server_addr_bytes[2],
                server_addr_bytes[3], server_addr_bytes[4], server_addr_bytes[5]);
}

/*
 * Updates flow info to be "active" or "expired"
 */
static int
update_status(uint64_t elapsed_cycles, struct flow_info *data) {
        if (unlikely(data == NULL)) {
                return -1;
        }
        if ((elapsed_cycles - data->last_pkt_cycles) / rte_get_timer_hz() >= lb->expire_time) {
                data->is_active = 0;
        } else {
                data->is_active = 1;
        }

        return 0;
}

/*
 * Clears expired entries from the flow table
 */
static int
clear_entries(void) {
        if (unlikely(lb == NULL)) {
                return -1;
        }

        printf("Clearing expired entries\n");
        struct flow_info *data = NULL;
        struct onvm_ft_ipv4_5tuple *key = NULL;
        uint32_t next = 0;
        int ret = 0;

        while (onvm_ft_iterate(lb->ft, (const void **)&key, (void **)&data, &next) > -1) {
                if (update_status(lb->elapsed_cycles, data) < 0) {
                        return -1;
                }

                if (!data->is_active) {
                        ret = onvm_ft_remove_key(lb->ft, key);
                        lb->num_stored--;
                        if (ret < 0) {
                                printf("Key should have been removed, but was not\n");
                                lb->num_stored++;
                        }
                }
        }

        return 0;
}

/*
 * Adds an entry to the flow table. It first checks if the table is full, and
 * if so, it calls clear_entries() to free up space.
 */
static int
table_add_entry(struct onvm_ft_ipv4_5tuple *key, struct flow_info **flow) {
        struct flow_info *data = NULL;

        if (unlikely(key == NULL || lb == NULL)) {
                return -1;
        }

        if (TABLE_SIZE - 1 - lb->num_stored == 0) {
                int ret = clear_entries();
                if (ret < 0) {
                        return -1;
                }
        }

        int tbl_index = onvm_ft_add_key(lb->ft, key, (char **)&data);
        if (tbl_index < 0) {
                return -1;
        }

        lb->num_stored++;
        if (!strcmp(lb->policy,"random")) {
                time_t t;
                /* Intializes random number generator */
                srand((unsigned) time(&t));
                data->dest = rand() % lb->server_count;
        }
        else if (!strcmp(lb->policy,"rrobin")) {
                data->dest = lb->num_stored % lb->server_count;
        }
        else if (!strcmp(lb->policy,"weighted_random")) {
                time_t t;
                int i, wrand, cur_weight_sum;
                /* Intializes random number generator */
                srand((unsigned) time(&t));
                wrand = rand() % lb->total_weight;
                cur_weight_sum=0;
                for (i = 0; i < lb->server_count; i++) {
                        cur_weight_sum+=lb->weights[i];
                        if(wrand<=cur_weight_sum) {
                                data->dest=i;
                                break;
                        }
                }

        }
        
        
        data->last_pkt_cycles = lb->elapsed_cycles;
        data->is_active = 0;
        *flow = data;

        return 0;
}

/*
 * Looks up a packet hash to see if there is a matching key in the table.
 * If it finds one, it updates the metadata associated with the key entry,
 * and if it doesn't, it calls table_add_entry() to add it to the table.
 */
static int
table_lookup_entry(struct rte_mbuf *pkt, struct flow_info **flow) {
        struct flow_info *data = NULL;
        struct onvm_ft_ipv4_5tuple key;

        if (unlikely(pkt == NULL || lb == NULL || flow == NULL)) {
                return -1;
        }

        int ret = onvm_ft_fill_key_symmetric(&key, pkt);
        if (ret < 0)
                return -1;

        int tbl_index = onvm_ft_lookup_key(lb->ft, &key, (char **)&data);
        if (tbl_index == -ENOENT) {
                return table_add_entry(&key, flow);
        } else if (tbl_index < 0) {
                printf("Some other error occurred with the packet hashing\n");
                return -1;
        } else {
                data->last_pkt_cycles = lb->elapsed_cycles;
                *flow = data;
                return 0;
        }
}

static int
callback_handler(__attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx) {
        lb->elapsed_cycles = rte_get_tsc_cycles();

        if ((lb->elapsed_cycles - lb->last_cycles) / rte_get_timer_hz() > lb->expire_time) {
                lb->last_cycles = lb->elapsed_cycles;
        }

        return 0;
}

static int
packet_handler(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta,
               __attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx) {
        static uint32_t counter = 0;
        struct rte_ipv4_hdr *ip;
        struct rte_ether_hdr *ehdr;
        struct flow_info *flow_info;
        int i, ret;

        ehdr = onvm_pkt_ether_hdr(pkt);
        ip = onvm_pkt_ipv4_hdr(pkt);

        /* Ignore packets without ip header, also ignore packets with invalid ip */
        if (ip == NULL || ip->src_addr == 0 || ip->dst_addr == 0) {
                meta->action = ONVM_NF_ACTION_DROP;
                meta->destination = 0;
                return 0;
        }

        /*
         * Before hashing remove the Load Balancer ip from the pkt so that both
         * connections from client -> lbr and lbr <- server
         * will have the same hash
         */
        if (pkt->port == lb->client_port) {
                ip->dst_addr = 0;
        } else {
                ip->src_addr = 0;
        }

        /* Get the packet flow entry */
        ret = table_lookup_entry(pkt, &flow_info);
        if (ret == -1) {
                meta->action = ONVM_NF_ACTION_DROP;
                meta->destination = 0;
                return 0;
        }

        /* If the flow entry is new, save the client information */
        if (flow_info->is_active == 0) {
                flow_info->is_active = 1;
                for (i = 0; i < RTE_ETHER_ADDR_LEN; i++) {
                        flow_info->s_addr_bytes[i] = ehdr->s_addr.addr_bytes[i];
                }
        }

        if (pkt->port == lb->server_port) {
                if (onvm_get_macaddr(lb->client_port, &ehdr->s_addr) == -1) {
                        rte_exit(EXIT_FAILURE, "Failed to obtain MAC address\n");
                }
                for (i = 0; i < RTE_ETHER_ADDR_LEN; i++) {
                        ehdr->d_addr.addr_bytes[i] = flow_info->s_addr_bytes[i];
                }

                ip->src_addr = lb->ip_lb_client;
                meta->destination = lb->client_port;
        } else {
                if (onvm_get_macaddr(lb->server_port, &ehdr->s_addr) == -1) {
                        rte_exit(EXIT_FAILURE, "Failed to obtain MAC address\n");
                }
                for (i = 0; i < RTE_ETHER_ADDR_LEN; i++) {
                        ehdr->d_addr.addr_bytes[i] = lb->server[flow_info->dest].d_addr_bytes[i];
                }

                ip->dst_addr = rte_cpu_to_be_32(lb->server[flow_info->dest].d_ip);
                meta->destination = lb->server_port;
        }

        /* Changing the pkt ip header so we want to recalculate pkt checksums */
        onvm_pkt_set_checksums(pkt);

        meta->action = ONVM_NF_ACTION_OUT;

        if (++counter == print_delay) {
                do_stats_display(pkt);
                print_flow_info(flow_info);
                counter = 0;
        }

        return 0;
}

int
main(int argc, char *argv[]) {
        struct onvm_nf_local_ctx *nf_local_ctx;
        struct onvm_nf_function_table *nf_function_table;
        int arg_offset;
        const char *progname = argv[0];

        nf_local_ctx = onvm_nflib_init_nf_local_ctx();
        onvm_nflib_start_signal_handler(nf_local_ctx, NULL);

        nf_function_table = onvm_nflib_init_nf_function_table();
        nf_function_table->pkt_handler = &packet_handler;
        nf_function_table->user_actions = &callback_handler;

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

        lb = rte_calloc("state", 1, sizeof(struct loadbalance), 0);
        if (lb == NULL) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Unable to initialize NF lb struct");
        }

        if (parse_app_args(argc, argv, progname) < 0)
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");

        lb->ft = onvm_ft_create(TABLE_SIZE, sizeof(struct flow_info));
        if (lb->ft == NULL) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Unable to create flow table");
        }

        validate_iface_config();
        parse_backend_json_config();

        lb->expire_time = 32;
        lb->elapsed_cycles = rte_get_tsc_cycles();

        onvm_nflib_run(nf_local_ctx);

        onvm_nflib_stop(nf_local_ctx);
        onvm_ft_free(lb->ft);
        rte_free(lb);
        printf("If we reach here, program is ending\n");
        return 0;
}
