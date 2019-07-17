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
 ********************************************************************/

/************************************************************************************
* Filename:   NAPT.cpp
* Author:     Hongyi Huang(hhy17 AT mails.tsinghua.edu.cn), Bangwen Deng, Wenfei Wu
* Copyright:
* Disclaimer: This code is presented "as is" without any guarantees.
* Details:    This code is the implementation of state_firewall from NFD project,
              a C++ based NF developing framework designed by Wenfei's group
              from IIIS, Tsinghua University, China.
*************************************************************************************/

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

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>
#include <rte_ip.h>
#include <rte_mbuf.h>

#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"

#ifdef __cplusplus
}
#endif

// NFD ADD
#include <pcap.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "basic_classes.h"
#include "decode.h"

using namespace std;

#define NF_TAG "NAPT"

/* number of package between each print */
static uint32_t print_delay = 1000000;

static uint32_t destination;

/*******************************NFD features********************************/
Flow::Flow(u_char *packet, int totallength) {
        this->pkt = packet;
        int ethernet_header_length = 14;

        EtherHdr *e_hdr = (EtherHdr *)packet;
        if (ntohs(e_hdr->ether_type) == 0x8100)
                ethernet_header_length = 14 + 4;
        else
                ethernet_header_length = 14;
        IPHdr *ip_hdr = (IPHdr *)(packet + ethernet_header_length);
        int src_addr = ntohl(ip_hdr->ip_src.s_addr);
        this->headers[Sip] = new IP(src_addr, 32);
        int des_addr = ntohl(ip_hdr->ip_dst.s_addr);
        this->headers[Dip] = new IP(des_addr, 32);

        int ip_header_length = ((*(packet + ethernet_header_length)) & 0x0F);
        ip_header_length = ip_header_length * 4;
        TCPHdr *tcph = (TCPHdr *)(packet + ethernet_header_length + ip_header_length);
        this->headers[Sport] = new int(ntohs(tcph->th_sport));
        this->headers[Dport] = new int(ntohs(tcph->th_dport));
}
void
Flow::clean() {
        int ethernet_header_length = 14;
        u_char *packet = this->pkt;
        EtherHdr *e_hdr = (EtherHdr *)this->pkt;
        if (ntohs(e_hdr->ether_type) == 0x8100)
                ethernet_header_length = 14 + 4;
        else
                ethernet_header_length = 14;

        IPHdr *ip_hdr = (IPHdr *)(packet + ethernet_header_length);

        ip_hdr->ip_src.s_addr = htonl(((IP *)this->headers[Sip])->ip);
        ip_hdr->ip_dst.s_addr = htonl(((IP *)this->headers[Dip])->ip);

        int ip_header_length = ((*(packet + ethernet_header_length)) & 0x0F);
        ip_header_length = ip_header_length * 4;
        TCPHdr *tcph = (TCPHdr *)(packet + ethernet_header_length + ip_header_length);
        tcph->th_sport = htons(u_short(*((int *)this->headers[Sport])));
        tcph->th_dport = htons(u_short(*((int *)this->headers[Dport])));
}

long int _counter = 0;
unordered_map<string, int> F_Type::MAP = unordered_map<string, int>();
unordered_map<string, int> F_Type::MAP2 = unordered_map<string, int>();
int
process(Flow &f);
Flow f_glb;

/////time////
struct timeval begin_time;
struct timeval end_time;

void
_init_() {
        (new F_Type())->init();
}

IP _t1("192.168.0.0/16");
IP _t2("219.168.135.100/32");
int _t3=8;
int _t4=1;
State<IP> base(_t2);
State<int> port(_t3);
State<unordered_map<int,IP>> listIP(*(new unordered_map<int,IP>()));
State<unordered_map<int,int>> listPORT(*(new unordered_map<int,int>()));

int
process(Flow &f) {
        if (*((IP *)f.headers[Sip]) <= _t1) {
                listIP[f][port[f]] = (*(IP *)f.headers[Sip]);
                listPORT[f][port[f]] = (*(int *)f.headers[Sport]);
                (*(IP *)f.headers[Sip]) = base[f];
                (*(int *)f.headers[Sport]) = port[f];
                port[f] = port[f] + _t4;
        } else if ((*((IP *)f.headers[Sip]) != _t1 && (*(IP *)f.headers[Dip]) == base[f]) &&
                   (listIP[f].find((*(int *)f.headers[Dport])) != listIP[f].end())) {
                (*(IP *)f.headers[Dip]) = listIP[f][(*(int *)f.headers[Dport])];
                (*(int *)f.headers[Dport]) = listPORT[f][(*(int *)f.headers[Dport])];
        } else if (((*((IP *)f.headers[Sip]) != _t1) && (*(IP *)f.headers[Dip]) == base[f]) &&
                   (~(listIP[f].find((*(int *)f.headers[Dport])) != listIP[f].end()))) {
                return -1;
        } else if (*((IP *)f.headers[Sip]) != _t1 && (*(IP *)f.headers[Dip]) != base[f]) {
                return -1;
        }
        f.clean();
        return 0;
}

int
NAPT(u_char *pkt, int totallength) {
        f_glb = Flow(pkt, totallength);
        return process(f_glb);
}

void
stop() {
        gettimeofday(&end_time, NULL);

        double total = end_time.tv_sec - begin_time.tv_sec + (end_time.tv_usec - begin_time.tv_usec) / 1000000.0;

        printf("\n\n**************************************************\n");
        printf("%ld packets are processed\n", _counter);
        printf("NF runs for %f seconds\n", total);
        printf("**************************************************\n\n");
}

/**********************************************************************/

/*
 * Print a usage message
 */
static void
usage(const char *progname) {
        printf("Usage: %s [EAL args] -- [NF_LIB args] -- -d <destination> -p <print_delay>\n\n", progname);
}

/*
 * Parse the application arguments.
 */
static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c, dst_flag = 0;

        while ((c = getopt(argc, argv, "d:p:")) != -1) {
                switch (c) {
                        case 'd':
                                destination = strtoul(optarg, NULL, 10);
                                dst_flag = 1;
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

        if (!dst_flag) {
                RTE_LOG(INFO, APP, "NAPT NF requires destination flag -d.\n");
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
do_stats_display(struct rte_mbuf *pkt) {
        const char clr[] = {27, '[', '2', 'J', '\0'};
        const char topLeft[] = {27, '[', '1', ';', '1', 'H', '\0'};
        static uint64_t pkt_process = 0;
        struct ipv4_hdr *ip;

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

static int
packet_handler(struct rte_mbuf *buf, struct onvm_pkt_meta *meta,
               __attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx) {
        static uint32_t counter = 0;
        if (++counter == print_delay) {
                do_stats_display(buf);
                counter = 0;
        }
        ////////add NFD NF process here!////////////////
        _counter++;
        u_char * pkt = rte_pktmbuf_mtod(buf, u_char*);
        int length = (int)buf->pkt_len;
        int ok;

        ok = NAPT(pkt, length);

        if (ok == -1) {
                meta->action = ONVM_NF_ACTION_DROP;
        } else {
                meta->action = ONVM_NF_ACTION_TONF;
                meta->destination = destination;
        }
        ////////////////////////////////////////////////
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

        // NFD begin
        _init_();
        // NFD end

        if (parse_app_args(argc, argv, progname) < 0) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        }

        // NFD begin
        gettimeofday(&begin_time, NULL);
        // NFD end

        onvm_nflib_run(nf_local_ctx);

        onvm_nflib_stop(nf_local_ctx);
        stop();
        printf("If we reach here, program is ending\n");

        return 0;
}
