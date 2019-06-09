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
 * arp_response.c - an example using onvm. If it receives an ARP packet, send a response.
 ********************************************************************/

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

#include <rte_arp.h>
#include <rte_common.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>

#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"

#define NF_TAG "arp_response"

#define PKTMBUF_POOL_NAME "MProc_pktmbuf_pool"

struct state_info {
        struct rte_mempool *pktmbuf_pool;
        uint16_t nf_destination;
        uint32_t *source_ips;
        int print_flag;
};

struct state_info *state_info;

/* shared data structure containing host port info */
extern struct port_info *ports;

/*Prints a usage message */
static void
usage(const char *progname) {
        printf("Usage:\n");
        printf("%s [EAL args] -- [NF Lib args] -- -d <destination_id> -s <source_ip> [-p enable printing]\n", progname);
        printf("%s -F <CONFIG_FILE.json> [EAL args] -- [NF_LIB args] -- [NF args]>\n\n", progname);
        printf("Flags:\n");
        printf(
            " - `-d <destination_id>`: the NF will send non-ARP packets to the NF at this service ID, e.g. `-d 2` "
            "sends packets to service ID 2\n");
        printf(
            " - `-s <source_ip_list>`: the NF will map each comma separated IP (no spaces) to the corresponding port. "
            "Example: `-s 10.0.0.31,11.0.0.31` maps port 0 to 10.0.0.31, and port 1 to 11.0.0.31. If 0.0.0.0 is "
            "inputted, the IP will be 0. If too few IPs are inputted, the remaining ports will be ignored.\n");
        printf(" - `-p`: Enables printing of log information\n");
}

/* Parse how many IPs are in the input string */
static int
get_ip_count(char *input_string, const char *delim) {
        int ip_count = 0;
        char *token = NULL;
        char *buffer = NULL;
        char *ip_string = NULL;
        size_t length = sizeof(input_string);

        if (input_string == NULL || delim == NULL) {
                return -1;
        }

        ip_string = rte_calloc("Copy of IP String", sizeof(input_string), sizeof(char), 0);
        if (ip_string == NULL) {
                RTE_LOG(INFO, APP, "Unable to allocate space for IP string");
                return -1;
        }

        strncpy(ip_string, input_string, length);
        token = strtok_r(ip_string, delim, &buffer);

        while (token != NULL) {
                ++ip_count;
                token = strtok_r(NULL, delim, &buffer);
        }

        return ip_count;
}

/*Parses through app args, crashes if the 2 required args aren't set */
static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c = -1;
        int dst_flag = 0;
        int ip_flag = 0;
        int num_ips = 0;
        int current_ip = 0;
        int result = 0;
        const char delim[2] = ",";
        char *token;
        char *buffer;
        state_info->print_flag = 0;

        state_info->source_ips = rte_calloc("Array of decimal IPs", ports->num_ports, sizeof(uint32_t), 0);
        if (state_info->source_ips == NULL) {
                RTE_LOG(INFO, APP, "Unable to initialize source IP array\n");
                return -1;
        }

        while ((c = getopt(argc, argv, "d:s:p")) != -1) {
                switch (c) {
                        case 'd':
                                state_info->nf_destination = strtoul(optarg, NULL, 10);
                                dst_flag = 1;
                                RTE_LOG(INFO, APP, "Sending packets to service ID %d\n", state_info->nf_destination);
                                break;
                        case 's':
                                num_ips = get_ip_count(optarg, delim);
                                if (num_ips > ports->num_ports) {
                                        RTE_LOG(INFO, APP, "Too many IPs were entered!\n");
                                        return -1;
                                }

                                if (num_ips < 0) {
                                        RTE_LOG(INFO, APP, "Invalid IP pointer\n");
                                        return -1;
                                }

                                token = strtok_r(optarg, delim, &buffer);
                                while (token != NULL) {
                                        result = onvm_pkt_parse_ip(token, &state_info->source_ips[current_ip]);
                                        if (result < 0) {
                                                RTE_LOG(INFO, APP, "Invalid IP entered");
                                                return -1;
                                        }
                                        ++current_ip;
                                        token = strtok_r(NULL, delim, &buffer);
                                }

                                ip_flag = 1;
                                break;
                        case 'p':
                                state_info->print_flag = 1;
                                break;
                        case '?':
                                usage(progname);
                                if (optopt == 'd')
                                        RTE_LOG(INFO, APP, "Option -%c requires an argument\n", optopt);
                                else if (optopt == 'd')
                                        RTE_LOG(INFO, APP, "Option -%c requires an argument\n", optopt);
                                else
                                        RTE_LOG(INFO, APP, "Unknown option character\n");

                                return -1;
                        default:
                                usage(progname);
                                return -1;
                }
        }

        if (!dst_flag) {
                RTE_LOG(INFO, APP, "ARP Response NF needs a destination NF service ID with the -d flag\n");
                return -1;
        }

        if (!ip_flag) {
                RTE_LOG(INFO, APP, "ARP Response NF needs comma separated IPs with the -s flag\n");
                return -1;
        }

        return optind;
}

/* Creates an mbuf ARP reply pkt- fields are set according to info passed in.
 * For RFC about ARP, see https://tools.ietf.org/html/rfc826
 * RETURNS 0 if success, -1 otherwise */
static int
send_arp_reply(int port, struct ether_addr *tha, uint32_t tip, struct onvm_nf *nf) {
        struct rte_mbuf *out_pkt = NULL;
        struct onvm_pkt_meta *pmeta = NULL;
        struct ether_hdr *eth_hdr = NULL;
        struct arp_hdr *out_arp_hdr = NULL;

        size_t pkt_size = 0;

        if (tha == NULL) {
                return -1;
        }

        out_pkt = rte_pktmbuf_alloc(state_info->pktmbuf_pool);
        if (out_pkt == NULL) {
                rte_free(out_pkt);
                return -1;
        }

        pkt_size = sizeof(struct ether_hdr) + sizeof(struct arp_hdr);
        out_pkt->data_len = pkt_size;
        out_pkt->pkt_len = pkt_size;

        // SET ETHER HEADER INFO
        eth_hdr = onvm_pkt_ether_hdr(out_pkt);
        ether_addr_copy(&ports->mac[port], &eth_hdr->s_addr);
        eth_hdr->ether_type = rte_cpu_to_be_16(ETHER_TYPE_ARP);
        ether_addr_copy(tha, &eth_hdr->d_addr);

        // SET ARP HDR INFO
        out_arp_hdr = rte_pktmbuf_mtod_offset(out_pkt, struct arp_hdr *, sizeof(struct ether_hdr));

        out_arp_hdr->arp_hrd = rte_cpu_to_be_16(ARP_HRD_ETHER);
        out_arp_hdr->arp_pro = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
        out_arp_hdr->arp_hln = 6;
        out_arp_hdr->arp_pln = sizeof(uint32_t);
        out_arp_hdr->arp_op = rte_cpu_to_be_16(ARP_OP_REPLY);

        ether_addr_copy(&ports->mac[port], &out_arp_hdr->arp_data.arp_sha);
        out_arp_hdr->arp_data.arp_sip = state_info->source_ips[ports->id[port]];

        out_arp_hdr->arp_data.arp_tip = tip;
        ether_addr_copy(tha, &out_arp_hdr->arp_data.arp_tha);

        // SEND PACKET OUT/SET METAINFO
        pmeta = onvm_get_pkt_meta(out_pkt);
        pmeta->destination = port;
        pmeta->action = ONVM_NF_ACTION_OUT;

        return onvm_nflib_return_pkt(nf, out_pkt);
}

static int
packet_handler(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta,
               __attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx) {
        struct ether_hdr *eth_hdr = onvm_pkt_ether_hdr(pkt);
        struct arp_hdr *in_arp_hdr = NULL;
        int result = -1;

        /*
         * First check if pkt is of type ARP:
         * Then whether its an ARP REQUEST
         *      if packet target IP matches machine IP send ARP REPLY
         * If its an ARP REPLY send to dest
         * Ignore (fwd to dest) other opcodes
         */
        if (rte_cpu_to_be_16(eth_hdr->ether_type) == ETHER_TYPE_ARP) {
                in_arp_hdr = rte_pktmbuf_mtod_offset(pkt, struct arp_hdr *, sizeof(struct ether_hdr));
                switch (rte_cpu_to_be_16(in_arp_hdr->arp_op)) {
                        case ARP_OP_REQUEST:
                                if (rte_be_to_cpu_32(in_arp_hdr->arp_data.arp_tip) ==
                                                state_info->source_ips[ports->id[pkt->port]]) {
                                        result = send_arp_reply(pkt->port, &eth_hdr->s_addr,
                                                                in_arp_hdr->arp_data.arp_sip, nf_local_ctx->nf);
                                        if (state_info->print_flag) {
                                                printf("ARP Reply From Port %d (ID %d): %d\n", pkt->port,
                                                       ports->id[pkt->port], result);
                                        }
                                        meta->action = ONVM_NF_ACTION_DROP;
                                        return 0;
                                }
                                break;
                        case ARP_OP_REPLY:
                                /* Here we can potentially save the information */
                                break;
                        default:
                                if (state_info->print_flag) {
                                        printf("ARP with opcode %d, port %d (ID %d) DROPPED\n",
                                               rte_cpu_to_be_16(in_arp_hdr->arp_op), pkt->port, ports->id[pkt->port]);
                                }
                }
        }

        meta->destination = state_info->nf_destination;
        meta->action = ONVM_NF_ACTION_TONF;
        return 0;
}

int
main(int argc, char *argv[]) {
        int arg_offset;
        struct onvm_nf_local_ctx *nf_local_ctx;
        struct onvm_nf_function_table *nf_function_table;
        const char *progname = argv[0];

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

        state_info = rte_calloc("state", 1, sizeof(struct state_info), 0);
        if (state_info == NULL) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Unable to initialize NF state");
        }

        state_info->pktmbuf_pool = rte_mempool_lookup(PKTMBUF_POOL_NAME);
        if (state_info->pktmbuf_pool == NULL) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Cannot find mbuf pool!\n");
        }

        if (parse_app_args(argc, argv, progname) < 0) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments");
        }

        onvm_nflib_run(nf_local_ctx);

        onvm_nflib_stop(nf_local_ctx);
        printf("If we reach here, program is ending\n");
        return 0;
}
