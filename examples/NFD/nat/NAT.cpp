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
* Filename:   NAT.cpp
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

#define NF_TAG "NAT"

/* Struct that contains information about this NF */
struct onvm_nf_info *nf_info;

/* number of package between each print */
static uint32_t print_delay = 1000000;

static uint32_t destination;

/*******************************NFD features********************************/
Flow::Flow(u_char *packet, int totallength) {
        /*Decoding*/
        this->pkt = packet;
        struct ether_header *eth_header;
        eth_header = (struct ether_header *)packet;

        /* Pointers to start point of various headers */
        u_char *ip_header;
        /* Header lengths in bytes */
        int ethernet_header_length = 14; /* Doesn't change */
        int ip_header_length;
        ip_header = packet + ethernet_header_length;
        ip_header_length = ((*ip_header) & 0x0F);
        ip_header_length = ip_header_length * 4;
        int src_addr, des_addr;
        src_addr = ntohl(*((int *)(ip_header + 12)));
        des_addr = ntohl(*((int *)(ip_header + 16)));
        this->field_value["sip"] = new IP(src_addr, 32);
        this->field_value["dip"] = new IP(des_addr, 32);
        this->field_value["tag"] = new int(0);
        this->field_value["iplen"] = new int(totallength);
        short protocol = ntohs(*(short *)(ip_header + 9));
        this->field_value["UDP"] = new int((protocol == IPPROTO_UDP) ? 1 : 0);

        /*TCP layer*/
        TCPHdr *tcph = (TCPHdr *)(ip_header + ip_header_length);
        this->field_value["sport"] = new int(ntohs(tcph->th_sport));
        this->field_value["dport"] = new int(ntohs(tcph->th_dport));
        /*URG ACK PSH RST SYN FIN*/
        this->field_value["flag_fin"] = new int(ntohs(tcph->th_flags) & TH_FIN);
        this->field_value["flag_syn"] = new int(ntohs(tcph->th_flags) & TH_SYN);
        this->field_value["flag_ack"] = new int(ntohs(tcph->th_flags) & TH_ACK);
}
void
Flow::clean() {
        /*Encoding*/
        struct ether_header *eth_header;
        eth_header = (struct ether_header *)this->pkt;
        int ethernet_header_length = 14; /* Doesn't change */
        int ip_header_length;
        u_char *ip_header;
        ip_header = (u_char *)eth_header + ethernet_header_length;
        ip_header_length = ((*ip_header) & 0x0F);
        ip_header_length = ip_header_length * 4;

        *((int *)(ip_header + 12)) = htonl(*((int *)this->field_value["sip"]));
        *((int *)(ip_header + 16)) = htonl(*((int *)this->field_value["dip"]));
        // this->field_value["iplen"] = new int(totallength);

        /*TCP layer*/
        TCPHdr *tcph = (TCPHdr *)(ip_header + ip_header_length);
        tcph->th_sport = u_short(*((int *)this->field_value["sport"]));
        tcph->th_dport = u_short(*((int *)this->field_value["dport"]));
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
int _t3 = 12000;
int _t4 = 1;
State<IP> base(_t2);
State<int> port(_t3);
State<unordered_map<IP, unordered_map<int, int>>> mapping(*(new unordered_map<IP, unordered_map<int, int>>()));
State<unordered_map<int, IP>> port_ip(*(new unordered_map<int, IP>()));
State<unordered_map<int, int>> port_port(*(new unordered_map<int, int>()));

int
process(Flow &f) {
        if ((*((IP *)f["sip"]) <= _t1) &&
            (~(mapping[f].find((*(IP *)f["sip"])) != mapping[f].end()) &&
             (mapping[f].find((*(IP *)f["sip"])) != mapping[f].end() &&
              ~(mapping[f][(*(IP *)f["sip"])].find((*(int *)f["sport"])) != mapping[f][(*(IP *)f["sip"])].end())))) {
                mapping[f][(*(IP *)f["sip"])][(*(int *)f["sport"])] = port[f];
                port_ip[f][port[f]] = (*(IP *)f["sip"]);
                port_port[f][port[f]] = (*(int *)f["sport"]);
                (*(IP *)f["sip"]) = base[f];
                (*(int *)f["sport"]) = port[f];
                port[f] = port[f] + _t4;
        } else if ((*((IP *)f["sip"]) <= _t1) && (mapping[f].find((*(IP *)f["sip"])) != mapping[f].end() &&
                                                  ~(mapping[f][(*(IP *)f["sip"])].find((*(int *)f["sport"])) !=
                                                    mapping[f][(*(IP *)f["sip"])].end()))) {
                (*(IP *)f["sip"]) = base[f];
                (*(int *)f["sport"]) = mapping[f][(*(IP *)f["sip"])][(*(int *)f["sport"])];
        } else if ((*((IP *)f["sip"]) != _t1 && (*(IP *)f["dip"]) == base[f]) &&
                   (port_port[f].find((*(int *)f["dport"])) != port_port[f].end())) {
                (*(IP *)f["dip"]) = port_ip[f][(*(int *)f["dport"])];
                (*(int *)f["dport"]) = port_port[f][(*(int *)f["dport"])];
        } else if (*((IP *)f["sip"]) != _t1 && (*(IP *)f["dip"]) != base[f]) {
                return -1;
        }
        f.clean();
        return 0;
}

int
NAT(u_char *pkt, int totallength) {
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
                RTE_LOG(INFO, APP, "NAT NF requires destination flag -d.\n");
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
packet_handler(struct rte_mbuf *buf, struct onvm_pkt_meta *meta, __attribute__((unused)) struct onvm_nf_info *nf_info) {
        static uint32_t counter = 0;
        if (++counter == print_delay) {
                do_stats_display(buf);
                counter = 0;
        }
        ////////add NFD NF process here!////////////////
        _counter++;
        u_char *pkt = (u_char *)buf->buf_addr;
        int length = (int)buf->pkt_len;
        int ok;

        ok = NAT(pkt, length);

        meta->action = ONVM_NF_ACTION_TONF;
        meta->destination = destination;
        ////////////////////////////////////////////////
        return 0;
}

int
main(int argc, char *argv[]) {
        int arg_offset;

        const char *progname = argv[0];

        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG, &nf_info)) < 0)
                return -1;
        argc -= arg_offset;
        argv += arg_offset;

        // NFD begin
        _init_();
        // NFD end

        if (parse_app_args(argc, argv, progname) < 0) {
                onvm_nflib_stop(nf_info);
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        }

        // NFD begin
        gettimeofday(&begin_time, NULL);
        // NFD end

        onvm_nflib_run(nf_info, &packet_handler);

        stop();
        printf("If we reach here, program is ending\n");

        return 0;
}
