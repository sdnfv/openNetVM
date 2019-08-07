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
 * ndpi_stats.c - an example using onvm, nDPI. Inspect packets using nDPI
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

#include <rte_common.h>
#include <rte_ip.h>
#include <rte_mbuf.h>

#include <pcap/pcap.h>
#include "ndpi_main.h"
#include "ndpi_util.h"

#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"

#define NF_TAG "ndpi_stat"
#define TICK_RESOLUTION 1000

/* shared data structure containing host port info */
extern struct port_info *ports;

/* user defined settings */
static uint32_t destination = (uint16_t)-1;

/* pcap stucts */
const uint16_t MAX_SNAPLEN = (uint16_t)-1;
pcap_t *pd;

/* nDPI structs */
struct ndpi_detection_module_struct *module;
struct ndpi_workflow *workflow;
uint32_t current_ndpi_memory = 0, max_ndpi_memory = 0;
static u_int8_t quiet_mode = 0;
static u_int16_t decode_tunnels = 0;
static FILE *results_file = NULL;
static struct timeval begin, end;

/* nDPI methods */
void
setup_ndpi(void);
char *
formatTraffic(float numBits, int bits, char *buf);
char *
formatPackets(float numPkts, char *buf);
static void
node_proto_guess_walker(const void *node, ndpi_VISIT which, int depth, void *user_data);
static void
print_results(void);

/**
 * Source https://github.com/ntop/nDPI ndpiReader.c
 * @brief Traffic stats format
 */
char *
formatTraffic(float numBits, int bits, char *buf) {
        char unit;

        if (bits)
                unit = 'b';
        else
                unit = 'B';

        if (numBits < 1024) {
                snprintf(buf, 32, "%lu %c", (unsigned long)numBits, unit);
        } else if (numBits < (1024 * 1024)) {
                snprintf(buf, 32, "%.2f K%c", (float)(numBits) / 1024, unit);
        } else {
                float tmpMBits = ((float)numBits) / (1024 * 1024);

                if (tmpMBits < 1024) {
                        snprintf(buf, 32, "%.2f M%c", tmpMBits, unit);
                } else {
                        tmpMBits /= 1024;

                        if (tmpMBits < 1024) {
                                snprintf(buf, 32, "%.2f G%c", tmpMBits, unit);
                        } else {
                                snprintf(buf, 32, "%.2f T%c", (float)(tmpMBits) / 1024, unit);
                        }
                }
        }

        return (buf);
}

/**
 * Source https://github.com/ntop/nDPI ndpiReader.c
 * @brief Packets stats format
 */
char *
formatPackets(float numPkts, char *buf) {
        if (numPkts < 1000) {
                snprintf(buf, 32, "%.2f", numPkts);
        } else if (numPkts < (1000 * 1000)) {
                snprintf(buf, 32, "%.2f K", numPkts / 1000);
        } else {
                numPkts /= (1000 * 1000);
                snprintf(buf, 32, "%.2f M", numPkts);
        }

        return (buf);
}

/*
 * Print a usage message
 */
static void
usage(const char *progname) {
        printf("Usage:\n");
        printf("%s [EAL args] -- [NF_LIB args] -- -d <destination_nf> -w <output_file>\n", progname);
        printf("%s -F <CONFIG_FILE.json> [EAL args] -- [NF_LIB args] -- [NF args]\n\n", progname);
        printf("Flags:\n");
        printf(" - `-w <file_name>`: result file name to write to.\n");
        printf(" - `-d <nf_id>`: OPTIONAL destination NF to send packets to\n");
}

/*
 * Parse the application arguments.
 */
static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c;

        while ((c = getopt(argc, argv, "d:w:")) != -1) {
                switch (c) {
                        case 'w':
                                results_file = fopen(strdup(optarg), "w");
                                if (results_file == NULL) {
                                        RTE_LOG(INFO, APP, "Error in opening result file\n");
                                        return -1;
                                }
                                break;
                        case 'd':
                                destination = strtoul(optarg, NULL, 10);
                                RTE_LOG(INFO, APP, "destination nf = %d\n", destination);
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

void
setup_ndpi(void) {
        pd = pcap_open_dead(DLT_EN10MB, MAX_SNAPLEN);

        NDPI_PROTOCOL_BITMASK all;
        struct ndpi_workflow_prefs prefs;

        memset(&prefs, 0, sizeof(prefs));
        prefs.decode_tunnels = decode_tunnels;
        prefs.num_roots = NUM_ROOTS;
        prefs.max_ndpi_flows = MAX_NDPI_FLOWS;
        prefs.quiet_mode = quiet_mode;

        workflow = ndpi_workflow_init(&prefs, pd);

        NDPI_BITMASK_SET_ALL(all);
        ndpi_set_protocol_detection_bitmask2(workflow->ndpi_struct, &all);

        memset(workflow->stats.protocol_counter, 0, sizeof(workflow->stats.protocol_counter));
        memset(workflow->stats.protocol_counter_bytes, 0, sizeof(workflow->stats.protocol_counter_bytes));
        memset(workflow->stats.protocol_flows, 0, sizeof(workflow->stats.protocol_flows));
}

/*
 * Source https://github.com/ntop/nDPI ndpiReader.c
 * Modified for single workflow
 */
static void
node_proto_guess_walker(const void *node, ndpi_VISIT which, int depth, void *user_data) {
        struct ndpi_flow_info *flow = *(struct ndpi_flow_info **)node;
        u_int16_t thread_id = *((u_int16_t *)user_data);

        if ((which == ndpi_preorder) || (which == ndpi_leaf)) { /* Avoid walking the same node multiple times */
                if ((!flow->detection_completed) && flow->ndpi_flow)
                        flow->detected_protocol = ndpi_detection_giveup(workflow->ndpi_struct, flow->ndpi_flow);

                process_ndpi_collected_info(workflow, flow);
                workflow->stats.protocol_counter[flow->detected_protocol.app_protocol] +=
                    flow->src2dst_packets + flow->dst2src_packets;
                workflow->stats.protocol_counter_bytes[flow->detected_protocol.app_protocol] +=
                    flow->src2dst_bytes + flow->dst2src_bytes;
                workflow->stats.protocol_flows[flow->detected_protocol.app_protocol]++;
        }
}

/*
 * Source https://github.com/ntop/nDPI ndpiReader.c
 * Simplified nDPI reader result output for single workflow
 */
static void
print_results(void) {
        u_int32_t i;
        u_int32_t avg_pkt_size = 0;
        u_int64_t tot_usec;

        if (workflow->stats.total_wire_bytes == 0)
                return;

        for (i = 0; i < NUM_ROOTS; i++) {
                ndpi_twalk(workflow->ndpi_flows_root[i], node_proto_guess_walker, 0);
        }

        tot_usec = end.tv_sec * 1000000 + end.tv_usec - (begin.tv_sec * 1000000 + begin.tv_usec);

        printf("\nTraffic statistics:\n");
        printf("\tEthernet bytes:        %-13llu (includes ethernet CRC/IFC/trailer)\n",
               (long long unsigned int)workflow->stats.total_wire_bytes);
        printf("\tDiscarded bytes:       %-13llu\n", (long long unsigned int)workflow->stats.total_discarded_bytes);
        printf("\tIP packets:            %-13llu of %llu packets total\n",
               (long long unsigned int)workflow->stats.ip_packet_count,
               (long long unsigned int)workflow->stats.raw_packet_count);
        /* In order to prevent Floating point exception in case of no traffic*/
        if (workflow->stats.total_ip_bytes && workflow->stats.raw_packet_count)
                avg_pkt_size = (unsigned int)(workflow->stats.total_ip_bytes / workflow->stats.raw_packet_count);
        printf("\tIP bytes:              %-13llu (avg pkt size %u bytes)\n",
               (long long unsigned int)workflow->stats.total_ip_bytes, avg_pkt_size);
        printf("\tUnique flows:          %-13u\n", workflow->stats.ndpi_flow_count);

        printf("\tTCP Packets:           %-13lu\n", (unsigned long)workflow->stats.tcp_count);
        printf("\tUDP Packets:           %-13lu\n", (unsigned long)workflow->stats.udp_count);
        printf("\tVLAN Packets:          %-13lu\n", (unsigned long)workflow->stats.vlan_count);
        printf("\tMPLS Packets:          %-13lu\n", (unsigned long)workflow->stats.mpls_count);
        printf("\tPPPoE Packets:         %-13lu\n", (unsigned long)workflow->stats.pppoe_count);
        printf("\tFragmented Packets:    %-13lu\n", (unsigned long)workflow->stats.fragmented_count);
        printf("\tMax Packet size:       %-13u\n", workflow->stats.max_packet_len);
        printf("\tPacket Len < 64:       %-13lu\n", (unsigned long)workflow->stats.packet_len[0]);
        printf("\tPacket Len 64-128:     %-13lu\n", (unsigned long)workflow->stats.packet_len[1]);
        printf("\tPacket Len 128-256:    %-13lu\n", (unsigned long)workflow->stats.packet_len[2]);
        printf("\tPacket Len 256-1024:   %-13lu\n", (unsigned long)workflow->stats.packet_len[3]);
        printf("\tPacket Len 1024-1500:  %-13lu\n", (unsigned long)workflow->stats.packet_len[4]);
        printf("\tPacket Len > 1500:     %-13lu\n", (unsigned long)workflow->stats.packet_len[5]);

        if (tot_usec > 0) {
                char buf[32], buf1[32], when[64];
                float t = (float)(workflow->stats.ip_packet_count * 1000000) / (float)tot_usec;
                float b = (float)(workflow->stats.total_wire_bytes * 8 * 1000000) / (float)tot_usec;
                float traffic_duration;
                /* This currently assumes traffic starts to flow instantly */
                traffic_duration = tot_usec;
                printf("\tnDPI throughput:       %s pps / %s/sec\n", formatPackets(t, buf), formatTraffic(b, 1, buf1));
                t = (float)(workflow->stats.ip_packet_count * 1000000) / (float)traffic_duration;
                b = (float)(workflow->stats.total_wire_bytes * 8 * 1000000) / (float)traffic_duration;

                strftime(when, sizeof(when), "%d/%b/%Y %H:%M:%S", localtime(&begin.tv_sec));
                printf("\tAnalysis begin:        %s\n", when);
                strftime(when, sizeof(when), "%d/%b/%Y %H:%M:%S", localtime(&end.tv_sec));
                printf("\tAnalysis end:          %s\n", when);
                printf("\tTraffic throughput:    %s pps / %s/sec\n", formatPackets(t, buf), formatTraffic(b, 1, buf1));
                printf("\tTraffic duration:      %.3f sec\n", traffic_duration / 1000000);
        }

        for (i = 0; i <= ndpi_get_num_supported_protocols(workflow->ndpi_struct); i++) {
                if (workflow->stats.protocol_counter[i] > 0) {
                        if (results_file)
                                fprintf(results_file, "%s\t%llu\t%llu\t%u\n",
                                        ndpi_get_proto_name(workflow->ndpi_struct, i),
                                        (long long unsigned int)workflow->stats.protocol_counter[i],
                                        (long long unsigned int)workflow->stats.protocol_counter_bytes[i],
                                        workflow->stats.protocol_flows[i]);
                        printf(
                            "\t%-20s packets: %-13llu bytes: %-13llu "
                            "flows: %-13u\n",
                            ndpi_get_proto_name(workflow->ndpi_struct, i),
                            (long long unsigned int)workflow->stats.protocol_counter[i],
                            (long long unsigned int)workflow->stats.protocol_counter_bytes[i],
                            workflow->stats.protocol_flows[i]);
                }
        }
}

static int
packet_handler(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta,
               __attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx) {
        struct pcap_pkthdr pkt_hdr;
        struct timeval time;
        u_char *packet;
        ndpi_protocol prot;

        time.tv_usec = pkt->udata64;
        time.tv_sec = pkt->tx_offload;
        pkt_hdr.ts = time;
        pkt_hdr.caplen = rte_pktmbuf_data_len(pkt);
        pkt_hdr.len = rte_pktmbuf_data_len(pkt);
        packet = rte_pktmbuf_mtod(pkt, u_char *);

        prot = ndpi_workflow_process_packet(workflow, &pkt_hdr, packet);
        workflow->stats.protocol_counter[prot.app_protocol]++;
        workflow->stats.protocol_counter_bytes[prot.app_protocol] += pkt_hdr.len;

        if (destination != (uint16_t)-1) {
                meta->action = ONVM_NF_ACTION_TONF;
                meta->destination = destination;
        } else {
                meta->action = ONVM_NF_ACTION_OUT;
                meta->destination = pkt->port;

                if (onvm_pkt_swap_src_mac_addr(pkt, meta->destination, ports) != 0) {
                        RTE_LOG(INFO, APP, "ERROR: Failed to swap src mac with dst mac!\n");
                }
        }

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

        if (parse_app_args(argc, argv, progname) < 0) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        }

        setup_ndpi();

        gettimeofday(&begin, NULL);

        onvm_nflib_run(nf_local_ctx);

        gettimeofday(&end, NULL);
        print_results();

        if (!pd)
                pcap_close(pd);
        if (results_file)
                fclose(results_file);

        onvm_nflib_stop(nf_local_ctx);
        printf("If we reach here, program is ending\n");
        return 0;
}
