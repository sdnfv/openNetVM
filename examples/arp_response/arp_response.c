/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2017 George Washington University
 *            2015-2017 University of California Riverside
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

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_ether.h>
#include <rte_arp.h>
#include <rte_mempool.h>
#include <rte_malloc.h>

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

/* Struct that contains information about this NF */
struct onvm_nf_info *nf_info;
struct state_info *state_info;

/* shared data structure containing host port info */
extern struct port_info *ports;

/*Prints a usage message */
static void
usage(const char *progname) {
        printf("Usage: %s [EAL args] -- [NF Lib args] -- -d <destination_id> -s <source_ip> [-p enable printing]\n\n", progname);
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
                switch(c) {
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
send_arp_reply(int port, struct ether_addr *tha, uint32_t tip) {
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

        //SET ETHER HEADER INFO
        eth_hdr = onvm_pkt_ether_hdr(out_pkt);
        ether_addr_copy(&ports->mac[port], &eth_hdr->s_addr);
        eth_hdr->ether_type = rte_cpu_to_be_16(ETHER_TYPE_ARP);
        ether_addr_copy(tha, &eth_hdr->d_addr);

        //SET ARP HDR INFO
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

        //SEND PACKET OUT/SET METAINFO
        pmeta = onvm_get_pkt_meta(out_pkt);
        pmeta->destination = port;
        pmeta->action = ONVM_NF_ACTION_OUT;

        return onvm_nflib_return_pkt(out_pkt);
}

static int
packet_handler(struct rte_mbuf* pkt, struct onvm_pkt_meta* meta) {
        struct ether_hdr *eth_hdr = onvm_pkt_ether_hdr(pkt);
        struct arp_hdr *in_arp_hdr = NULL;
        int result = -1;

        //First checks to see if pkt is of type ARP, then whether the target IP of packet matches machine IP
        if (rte_cpu_to_be_16(eth_hdr->ether_type) == ETHER_TYPE_ARP) {
                in_arp_hdr = rte_pktmbuf_mtod_offset(pkt, struct arp_hdr *, sizeof(struct ether_hdr));
                if (in_arp_hdr->arp_data.arp_tip == state_info->source_ips[ports->id[pkt->port]]) {
                        result = send_arp_reply(pkt->port, &eth_hdr->s_addr, in_arp_hdr->arp_data.arp_sip);
                        if (state_info->print_flag) {
                                printf("ARP Reply From Port %d (ID %d): %d\n", pkt->port, ports->id[pkt->port], result);
                        }
                        meta->action = ONVM_NF_ACTION_DROP;
                        return 0;
                }
        }

        meta->destination = state_info->nf_destination;
        meta->action = ONVM_NF_ACTION_TONF;
        return 0;
}

int main(int argc, char *argv[]) {
        int arg_offset;
        const char *progname = argv[0];

        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG)) < 0) {
                return -1;
        }

        argc -= arg_offset;
        argv += arg_offset;

        state_info = rte_calloc("state", 1, sizeof(struct state_info), 0);
        if (state_info == NULL) {
                onvm_nflib_stop();
                rte_exit(EXIT_FAILURE, "Unable to initialize NF state");
        }

        state_info->pktmbuf_pool = rte_mempool_lookup(PKTMBUF_POOL_NAME);
        if (state_info->pktmbuf_pool == NULL) {
                onvm_nflib_stop();
                rte_exit(EXIT_FAILURE, "Cannot find mbuf pool!\n");
        }

        if (parse_app_args(argc, argv, progname) < 0) {
                onvm_nflib_stop();
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments");
        }

        onvm_nflib_run(nf_info, &packet_handler);
        printf("If we reach here, program is ending\n");
        return 0;
}
