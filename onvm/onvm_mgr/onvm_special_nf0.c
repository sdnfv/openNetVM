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
#include "onvm_special_nf0.h"
#include "onvm_comm_utils.h"
//#include "onvm_stats.h"
#include "onvm_ft_install.h"
//#include "onvm_pkt_helper.h"
#include "onvm_rsync.h"
#include "onvm_bfd.h"
#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_ether.h>
#include <rte_arp.h>
#ifdef ENABLE_VXLAN
#include "onvm_vxlan.h"
#ifdef ENABLE_ZOOKEEPER
#include "onvm_zookeeper.h"
#endif
#endif

/**************************Macros and Feature Definitions**********************/
/* Enable the ONVM_MGR to act as a 2-port bridge without any NFs */
#define ONVM_MGR_ACT_AS_2PORT_FWD_BRIDGE    // Work as bridge < without any NFs :: only testing purpose.. >
//#define SEND_DIRECT_ON_ALT_PORT

//#define DELAY_BEFORE_SEND
//#define DELAY_PER_PKT (5) //20micro seconds

static uint8_t keep_running = 1;
static struct onvm_nf *nf0_cl = NULL;
/*************************Local functions Declaration**************************/

/*******************************Helper functions********************************/
inline int set_packet_forwarind_actions(struct rte_mbuf *pkt, struct onvm_pkt_meta * meta);
inline int set_packet_forwarind_actions(struct rte_mbuf *pkt, struct onvm_pkt_meta * meta) {
        if (pkt->port == 0) {
                meta->destination = 0;
                if(ports->num_ports > 1 ) {
                        meta->destination = PRIMARY_OUT_PORT;
                        if(ports->down_status[PRIMARY_OUT_PORT]) {
                                meta->destination = SECONDARY_OUT_PORT;
                        }
                }
        }
        else {
                meta->destination = 0;
        }
        return 0;
}
#ifdef ONVM_MGR_ACT_AS_2PORT_FWD_BRIDGE
static int onv_pkt_send_on_alt_port(__attribute__((unused)) struct thread_info *rx, struct rte_mbuf *pkts[], uint16_t rx_count);
int send_direct_on_assigned_port(struct rte_mbuf *pkts[], uint16_t rx_count);
int send_direct_on_assigned_port(struct rte_mbuf *pkts[], uint16_t rx_count) {
        uint16_t i, j,sent_0;
        volatile struct tx_stats *tx_stats;
        tx_stats = &(ports->tx_stats);

        static struct packet_buf portpkts[RTE_MAX_ETHPORTS];
        struct onvm_pkt_meta *meta = NULL;

        for (i = 0; i < rx_count; i++) {
                meta = (struct onvm_pkt_meta*) &(((struct rte_mbuf*)pkts[i])->udata64);
                portpkts[meta->destination].buffer[portpkts[meta->destination].count++] = pkts[i];  //based on assigned action
                //portpkts[pkts[i]->port].buffer[portpkts[pkts[i]->port].count++] = pkts[i];    //based on packet port
        }


#ifdef DELAY_BEFORE_SEND
        usleep(DELAY_PER_PKT*count_0);
#endif
        for(i=0; i< RTE_MAX_ETHPORTS; i++) {
                if(portpkts[i].count) {
                        uint8_t port_id = i;
                        if(unlikely(port_id >= ports->num_ports)) {
                                for (j = 0; j < portpkts[i].count; i++) {
                                        onvm_pkt_drop(portpkts[i].buffer[j]);
                                }
                                //onvm_pkt_drop_batch(portpkts[i].buffer, portpkts[i].count);
                                tx_stats->tx_drop[port_id] += portpkts[i].count;
                                portpkts[i].count=0;
                                continue;
                        }
                        //printf("\n Sending %d packets on Port %d\n", portpkts[i].count, i);
                        sent_0 = rte_eth_tx_burst(port_id,
                                                0,//tx->queue_id,                           //using 5 in case of RSYNC otherwise 0
                                                (struct rte_mbuf**)portpkts[i].buffer,
                                                portpkts[i].count);
                        if (unlikely(sent_0 < portpkts[i].count)) {
                                for (j = sent_0; j < portpkts[i].count; i++) {
                                        onvm_pkt_drop(portpkts[i].buffer[j]);
                                }
                                tx_stats->tx_drop[port_id] += (portpkts[i].count - sent_0);
                        }
                        tx_stats->tx[port_id] += sent_0;
                        portpkts[i].count=0;
                }
        }
#ifdef DELAY_BEFORE_SEND
        usleep(DELAY_PER_PKT*count_1);
#endif
        return 0;
}
int send_direct_on_alt_port(struct rte_mbuf *pkts[], uint16_t rx_count);
int send_direct_on_alt_port(struct rte_mbuf *pkts[], uint16_t rx_count) {
        uint16_t i, sent_0,sent_1;
        volatile struct tx_stats *tx_stats;
        tx_stats = &(ports->tx_stats);

        struct rte_mbuf *pkts_0[PACKET_READ_SIZE];
        struct rte_mbuf *pkts_1[PACKET_READ_SIZE];
        uint16_t count_0=0, count_1=0;

        for (i = 0; i < rx_count; i++) {
                if (pkts[i]->port == 0) {
                        pkts_1[count_1++] = pkts[i];
                } else {
                        pkts_0[count_0++] = pkts[i];
                }
        }
#ifdef DELAY_BEFORE_SEND
        usleep(DELAY_PER_PKT*count_0);
#endif
        if(count_0) {
                uint8_t port_id = 0;
                sent_0 = rte_eth_tx_burst(port_id,
                                        0,//tx->queue_id,
                                        pkts_0,
                                        count_0);
                if (unlikely(sent_0 < count_0)) {
                        for (i = sent_0; i < count_0; i++) {
                                onvm_pkt_drop(pkts_0[i]);
                        }
                        tx_stats->tx_drop[port_id] += (count_0 - sent_0);
                }
                tx_stats->tx[port_id] += sent_0;
        }
#ifdef DELAY_BEFORE_SEND
        usleep(DELAY_PER_PKT*count_1);
#endif
        if(count_1) {
                uint8_t port_id = 0;
                if(ports->num_ports > 1 ) port_id=1;
                sent_1 = rte_eth_tx_burst(port_id,
                                        0,//tx->queue_id,
                                        pkts_1,
                                        count_1);
                if (unlikely(sent_1 < count_1)) {
                        for (i = sent_1; i < count_1; i++) {
                                onvm_pkt_drop(pkts_1[i]);
                        }
                        tx_stats->tx_drop[port_id] += (count_1 - sent_1);
                }
                tx_stats->tx[port_id] += sent_1;
        }
        return 0;
}
static int onv_pkt_send_on_alt_port(__attribute__((unused)) struct thread_info *rx, struct rte_mbuf *pkts[], uint16_t rx_count) {

        int ret = 0;
        int i = 0;
        struct onvm_pkt_meta *meta = NULL;
        struct rte_mbuf *pkt = NULL;
        int j = 0;
        struct rte_mbuf *pkts_out[PACKET_READ_SIZE];
        struct rte_mbuf *pkts_proc[PACKET_READ_SIZE];
        int pr_count=0;int enq_status=0;
        if (pkts == NULL || rx_count== 0)
                return ret;

#ifdef SEND_DIRECT_ON_ALT_PORT
        return send_direct_on_alt_port(pkts, rx_count);
#endif //SEND_DIRECT_ON_ALT_PORT

        /* Set Packet action to OUTPUT on Port and Push the packets directly to the Tx Ring of the Speacial NF[0] */
        for (i = 0; i < rx_count; i++) {
               meta = (struct onvm_pkt_meta*) &(((struct rte_mbuf*)pkts[i])->udata64);
               meta->src = 0;
               meta->chain_index = 0;
               pkt = (struct rte_mbuf*)pkts[i];
               meta->action = ONVM_NF_ACTION_OUT;
               meta->destination = pkts[i]->port;

               // Filter out BFD and RSYNC packets to avoid looping them around!
               struct ether_hdr *eth = rte_pktmbuf_mtod(pkts[i], struct ether_hdr *);
               if(ETHER_TYPE_RSYNC_DATA == rte_be_to_cpu_16(eth->ether_type)) {
                       //pkts_out[j++]=pkt;
                       //meta->action = ONVM_NF_ACTION_DROP;
                       onvm_pkt_drop(pkts[i]); continue;
               } else if (ETHER_TYPE_BFD == rte_be_to_cpu_16(eth->ether_type)) {
                       pkts_proc[pr_count++] = pkts[i]; continue;
               } else {
                       pkts_out[j++]=pkt;
               }

               if(ONVM_NF_ACTION_OUT == meta->action) {

#ifdef USE_SINGLE_NIC_PORT
                       meta->destination = pkt->port;
#else
#if 1
                       set_packet_forwarind_actions(pkt, meta);
#else
                       if (pkt->port == 0) {
                                meta->destination = 0;
                                if(ports->num_ports > 1 ) {
                                        meta->destination = PRIMARY_OUT_PORT;
                                        if(ports->down_status[PRIMARY_OUT_PORT]) {
                                                meta->destination = SECONDARY_OUT_PORT;
                                        }
                                }
                        }
                        else {
                                meta->destination = 0;
                        }
#endif
#endif
               }
        }

        //Make use of the internal NF[0]
        if(NULL == nf0_cl) nf0_cl = &nfs[0];
        // DO ONCE: Ensure destination NF is running and ready to receive packets
        if (!onvm_nf_is_valid(nf0_cl)) {
                start_special_nf0();
        }

        //Packets that demand processing
        if(pr_count) {
                enq_status = rte_ring_enqueue_bulk(nf0_cl->rx_q, (void **)pkts_proc, pr_count,NULL);
                if (0 == enq_status) { //if (-ENOBUFS == enq_status) {
                        onvm_pkt_drop_batch(pkts_proc,pr_count);
                        nf0_cl->stats.rx_drop += pr_count;
                }
                //onvm_pkt_drop_batch(pkts_drop,dr_count);
        }

#if 0
        //Push all packets directly to the NF[0]->tx_ring
        enq_status = rte_ring_enqueue_bulk(nf0_cl->tx_q, (void **)pkts_out, j);
        //int enq_status = rte_ring_enqueue_bulk(nf0_cl->tx_q, (void **)pkts, rx_count);
        if(0 == enq_status) { //if (-ENOBUFS == enq_status) {
                //printf("Enqueue to NF[0] Tx Buffer failed!!");
                onvm_pkt_drop_batch(pkts_out,j);
                nf0_cl->stats.tx_drop += j;
                //onvm_pkt_drop_batch(pkts,rx_count);
                //nf0_cl->stats.rx_drop += rx_count;
        }
#else
        send_direct_on_assigned_port(pkts_out,j);
#endif

        return ret;
}
#endif //ONVM_MGR_ACT_AS_2PORT_FWD_BRIDGE
/******************************************************************************/

/*********************** ARP RESPONSE HELPER FUNCTIONS ************************/
/* Creates an mbuf ARP reply pkt- fields are set according to info passed in.
 * For RFC about ARP, see https://tools.ietf.org/html/rfc826
 * RETURNS 0 if success, -1 otherwise */

struct state_info {
        struct rte_mempool *pktmbuf_pool;
        uint16_t nf_destination;
        uint32_t *source_ips;
        int print_flag;
};

/* Struct that contains information about MGR IFACE IPs */
struct state_info *state_info;

typedef struct onvm_arp_resp_args {
        const char* ipmap_file;      /* -s <service chain listings> */
        const char* ipmap_csv;       /* -b <IPv45Tuple Base Ip Address> */
        uint32_t max_ports;          /* -m <Maximum number of IP Addresses> */
}onvm_arp_resp_args_t;
static onvm_arp_resp_args_t arp_resp_info = {
        .ipmap_file = "ipmap.txt",
        .ipmap_csv = "10.0.0.3,10.10.0.3",
        .max_ports = 10,
};
static void
parse_port_ip_map(void) {
        const char delim[2] = ",";
        char *buffer;
        char *input = strdup(arp_resp_info.ipmap_csv);
        char* token = strtok_r((char*)input, delim, &buffer);
        int current_ip = 0, result =0;
        while (token != NULL) {
                result = onvm_pkt_parse_ip(token, &state_info->source_ips[current_ip]);
                if (result < 0) {
                        return;
                }
                printf("\n token=%s, state->source_ip[currentID=%d]=%d\n",token, current_ip,state_info->source_ips[current_ip]);
                printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
                                        ports->mac[current_ip].addr_bytes[0], ports->mac[current_ip].addr_bytes[1],
                                        ports->mac[current_ip].addr_bytes[2], ports->mac[current_ip].addr_bytes[3],
                                        ports->mac[current_ip].addr_bytes[4], ports->mac[current_ip].addr_bytes[5]);
                ++current_ip;

                token = strtok_r(NULL, delim, &buffer);
                if(current_ip >= ports->num_ports) break;
        }
        return;
}
static int
send_arp_reply_v2(int req_port, int arp_port, struct ether_addr *tha, uint32_t tip) {
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
        ether_addr_copy(&ports->mac[req_port], &eth_hdr->s_addr);
        eth_hdr->ether_type = rte_cpu_to_be_16(ETHER_TYPE_ARP);
        ether_addr_copy(tha, &eth_hdr->d_addr);

        //SET ARP HDR INFO
        out_arp_hdr = rte_pktmbuf_mtod_offset(out_pkt, struct arp_hdr *, sizeof(struct ether_hdr));

        out_arp_hdr->arp_hrd = rte_cpu_to_be_16(ARP_HRD_ETHER);
        out_arp_hdr->arp_pro = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
        out_arp_hdr->arp_hln = 6;
        out_arp_hdr->arp_pln = sizeof(uint32_t);
        out_arp_hdr->arp_op = rte_cpu_to_be_16(ARP_OP_REPLY);

        ether_addr_copy(&ports->mac[arp_port], &out_arp_hdr->arp_data.arp_sha);
        out_arp_hdr->arp_data.arp_sip = state_info->source_ips[ports->id[arp_port]];

        out_arp_hdr->arp_data.arp_tip = tip;
        ether_addr_copy(tha, &out_arp_hdr->arp_data.arp_tha);

        //SEND PACKET OUT/SET METAINFO
        pmeta = onvm_get_pkt_meta(out_pkt);
        pmeta->destination = req_port;
        pmeta->action = ONVM_NF_ACTION_OUT;

        int enq_status = rte_ring_enqueue_bulk(nf0_cl->tx_q, (void **)&out_pkt, 1,NULL);
        //if (-ENOBUFS == enq_status) {
        if (0 == enq_status) {
                onvm_pkt_drop_batch(&out_pkt,1);
                nf0_cl->stats.rx_drop += 1;
        }
        printf("\n ARP Reply packet sent on port [%d] for port [%d]!\n", req_port, arp_port);
        return 0; //onvm_nflib_return_pkt(out_pkt);
}
//static
int send_arp_reply(int port, struct ether_addr *tha, uint32_t tip);
//static
int send_arp_reply(int port, struct ether_addr *tha, uint32_t tip) {
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

        int enq_status = rte_ring_enqueue_bulk(nf0_cl->tx_q, (void **)&out_pkt, 1,NULL);
        if(0 == enq_status) { //if (-ENOBUFS == enq_status) {
                onvm_pkt_drop_batch(&out_pkt,1);
                nf0_cl->stats.rx_drop += 1;
        }
        printf("\n ARP Reply packet sent on port [%d]!\n", port);
        return 0; //onvm_nflib_return_pkt(out_pkt);
}
static int try_check_and_send_arp_response(struct rte_mbuf* pkt, struct onvm_pkt_meta* meta);
static int try_check_and_send_arp_response(struct rte_mbuf* pkt, struct onvm_pkt_meta* meta) {
        struct ether_hdr *eth_hdr = onvm_pkt_ether_hdr(pkt);
        struct arp_hdr *in_arp_hdr = NULL;
        int result = -1;
        if(NULL == eth_hdr) return -1;

        //First checks to see if pkt is of type ARP, then whether the target IP of packet matches machine IP
        if (rte_cpu_to_be_16(eth_hdr->ether_type) == ETHER_TYPE_ARP) {
                in_arp_hdr = rte_pktmbuf_mtod_offset(pkt, struct arp_hdr *, sizeof(struct ether_hdr));
                if(NULL == in_arp_hdr) return -1;
                int i = 0;
                for(;i<ports->num_ports;i++) {
                        //printf("\n checking for ARP Reply From Port %d (ID %d): [%u:%u]\n", pkt->port, ports->id[pkt->port], (unsigned) in_arp_hdr->arp_data.arp_tip, (unsigned) state_info->source_ips[ports->id[pkt->port]]);
                        if (in_arp_hdr->arp_data.arp_tip == state_info->source_ips[ports->id[i]]) { //if (in_arp_hdr->arp_data.arp_tip == state_info->source_ips[ports->id[pkt->port]]) {
                                //result = send_arp_reply(pkt->port, &eth_hdr->s_addr, in_arp_hdr->arp_data.arp_sip);
                                result = send_arp_reply_v2(pkt->port, i, &eth_hdr->s_addr, in_arp_hdr->arp_data.arp_sip);
                                if (state_info->print_flag) {
                                        printf("ARP Reply For Port [%d %d] to port [%d %d]: %d\n", i, ports->id[i], pkt->port, ports->id[pkt->port], result);
                                }
                                meta->action = ONVM_NF_ACTION_DROP;
                                return 0;
                        }
                }
        }
        return result;
}
static int onvm_special_nf_arp_responder_init(void) {
        state_info = rte_calloc("state_info", 1, sizeof(struct state_info), 0);
        if (state_info == NULL) {
                rte_exit(EXIT_FAILURE, "Unable to initialize NFMGR state info");
        }

        state_info->pktmbuf_pool = rte_mempool_lookup(PKTMBUF_POOL_NAME);
        if (state_info->pktmbuf_pool == NULL) {
                rte_exit(EXIT_FAILURE, "Cannot find mbuf pool!\n");
        }
        if(!ports || ! ports->num_ports) {
                //rte_exit(EXIT_FAILURE, "Cannot find any active ports!\n");
                return 0;
        }
        state_info->source_ips = rte_calloc("Array of decimal IPs", ports->num_ports, sizeof(uint32_t), 0);
        if (state_info->source_ips == NULL) {
                rte_exit(EXIT_FAILURE, "Unable to initialize source IP array [%d]\n", ports->num_ports);
        }
        parse_port_ip_map();
        return 0;
}
/*********************** ARP RESPONSE HELPER FUNCTIONS ************************/
#ifdef ENABLE_PCAP_CAPTURE
/*********************** PCAP DUMP FEATURE ************************************/
#include <pcap/pcap.h>
const uint16_t MAX_SNAPLEN = (uint16_t) -1;

#define USE_PCAP_ROTATION_BUFFERS
#ifndef USE_PCAP_ROTATION_BUFFERS
#define PCAP_FILE_NAME  "pcap_capture.pcap"
#define PACP_PER_PORT_FILE_NAME "pcap_capture_%d.pcap"
static inline const char *
get_pcap_file_name(unsigned port_id) {
        static char buffer[sizeof(PACP_PER_PORT_FILE_NAME) + 2];
        snprintf(buffer, sizeof(buffer) - 1, PACP_PER_PORT_FILE_NAME, port_id);
        return buffer;
}
typedef struct pcap_session_meta {
        uint64_t max_capture_size;
        uint64_t max_packet_count;
        uint32_t reset_log_period_in_ms;

        //char capture_file[64];
        const char* capture_file;
        uint8_t capture_port;
        pcap_t *pd;
        pcap_dumper_t *pcap_dumper;
        uint8_t is_active;
        uint64_t captured_size;
        uint32_t captured_packet_count;
        onvm_time_t capture_start_time;
        onvm_time_t capture_end_time;

}pcap_session_meta_t;

#else
#define MAX_PCAP_ROT_FILES  (2)
#define PCAP_FILE_NAME  "pcap_capture.pcap"
#define PACP_PER_PORT_FILE_NAME "pcap_capture_%d_%d.pcap"
static inline const char *
get_pcap_file_name(unsigned port_id, unsigned index) {
        static char buffer[sizeof(PACP_PER_PORT_FILE_NAME) + 6];
        snprintf(buffer, sizeof(buffer) - 1, PACP_PER_PORT_FILE_NAME, port_id,index);
        return buffer;
}
typedef struct pcap_log_meta{
        //char capture_file[64];
        const char* capture_file;
        pcap_t *pd;
        pcap_dumper_t *pcap_dumper;
        uint8_t is_active;
        uint64_t captured_size;
        uint32_t captured_packet_count;
        onvm_time_t capture_start_time;
        onvm_time_t capture_end_time;

}pcap_log_meta_t;
typedef struct pcap_session_meta {
        uint64_t max_capture_size;
        uint64_t max_packet_count;
        uint32_t reset_log_period_in_ms;
        uint8_t capture_port;
        uint8_t log_active_status;
        uint8_t ring_index;
        pcap_log_meta_t log[MAX_PCAP_ROT_FILES];

}pcap_session_meta_t;
#endif

//pcap_port_info[RTE_MAX_PORTS] -- can ideally extend to distinct files per port.
//Roughly 10Mpps => every 10 milli seconds = 10*(10*1000) packets and size of 10MB
#define PCAP_MAX_PACKET_COUNT   ((uint64_t)1000*1000*1000*10) //(100*1000) //(1000*1000*1000)
#define PCAP_MAX_CAPTURE_SIZE   ((uint64_t)1024*1024*1024*10) //((uint64_t)1024*1024*10) //((uint64_t)1024*1024*1024*1)
#define USE_PCAP_PER_OUT_PORT
#ifndef USE_PCAP_PER_OUT_PORT
pcap_session_meta_t pcap_info = {
                //.capture_file = "pcap_capture00.pcap"; //{'p','c','a','p','_','c','a','p','t','u','r','e','0','0','.','p','c','a','p','\0'},
                .max_capture_size = (uint64_t)1024*1024*1024*1/4,
                .max_packet_count = 1000*1000*1000,
                .capture_port = 0,
                .pd = NULL,
                .pcap_dumper = NULL,
                .captured_size =0,
                .captured_packet_count=0,
                .is_active=0
};
#else
pcap_session_meta_t pcap_infos[RTE_MAX_ETHPORTS];
#endif
typedef enum pcap_log_status_type {
        pcap_log_status_unused=0,
        pcap_log_status_active_logging=1,
        pcap_log_status_active_logging_stopped=2,
        pcap_log_status_replay_on=3,
        pcap_log_status_replay_finished=4,
}pcap_log_status_type_e;

#ifndef USE_PCAP_ROTATION_BUFFERS
inline int onvm_util_init_pacp_logger(__attribute__((unused)) int port, __attribute__((unused)) int mode) {
#ifndef USE_PCAP_PER_OUT_PORT
        if(pcap_info.pd) return EALREADY;
        //pcap_info.pd = pcap_open_dead(DLT_EN10MB, MAX_SNAPLEN);
        //pcap_info.pd = pcap_open_dead_with_tstamp_precision(DLT_RAW, MAX_SNAPLEN, PCAP_TSTAMP_PRECISION_NANO);
        pcap_info.pd = pcap_open_dead_with_tstamp_precision(DLT_EN10MB, MAX_SNAPLEN, PCAP_TSTAMP_PRECISION_NANO);
        if(pcap_info.pd == NULL) return 2;
        pcap_info.capture_file = get_pcap_file_name(port);
        pcap_info.pcap_dumper = pcap_dump_open(pcap_info.pd, (const char*)pcap_info.capture_file);
        if(NULL == pcap_info.pcap_dumper) return 3;

        pcap_info.captured_packet_count=0;
        pcap_info.captured_size=0;
        pcap_info.is_active=pcap_log_status_active_logging;
        pcap_info.capture_port=port;
#else
        if(pcap_infos[port].pd) return EALREADY;
        pcap_infos[port].pd = pcap_open_dead_with_tstamp_precision(DLT_EN10MB, MAX_SNAPLEN, PCAP_TSTAMP_PRECISION_NANO);
        if(pcap_infos[port].pd == NULL) return 2;
        pcap_infos[port].capture_file = get_pcap_file_name(port);
        pcap_infos[port].pcap_dumper = pcap_dump_open(pcap_infos[port].pd, (const char*)pcap_infos[port].capture_file);
        if(NULL == pcap_infos[port].pcap_dumper) return 3;

        pcap_infos[port].max_capture_size = PCAP_MAX_CAPTURE_SIZE;
        pcap_infos[port].max_packet_count = PCAP_MAX_PACKET_COUNT;
        pcap_infos[port].captured_packet_count=0;
        pcap_infos[port].captured_size=0;
        pcap_infos[port].is_active=pcap_log_status_active_logging;
        pcap_infos[port].capture_port=port;

#endif

        return 0;
}

inline int onvm_util_clear_pcap_log(__attribute__((unused)) int port) {
#ifndef USE_PCAP_PER_OUT_PORT
        //onvm_util_close_and_get_pdump_file(port);
        //return onvm_util_init_pacp_logger(port, 0);
        pcap_dump_close(pcap_info.pcap_dumper);

        pcap_info.pcap_dumper = pcap_dump_open(pcap_info.pd, (const char*)pcap_info.capture_file);

        pcap_info.captured_packet_count=0;
        pcap_info.captured_size=0;
        pcap_info.is_active=pcap_log_status_active_logging;
        pcap_info.capture_port=port;
#else
        pcap_dump_close(pcap_infos[port].pcap_dumper);

        pcap_infos[port].pcap_dumper = pcap_dump_open(pcap_infos[port].pd, (const char*)pcap_infos[port].capture_file);

        pcap_infos[port].captured_packet_count=0;
        pcap_infos[port].captured_size=0;
        pcap_infos[port].is_active=pcap_log_status_active_logging;
        pcap_infos[port].capture_port=port;
#endif
        return 0;
}
inline int onvm_util_log_packets(__attribute__((unused)) int port, struct rte_mbuf **pkts, __attribute__((unused)) uint64_t *ts_info, uint16_t nb_pkts) {
        unsigned i, cap_size=0;
        u_char *packet;
        struct pcap_pkthdr pkt_hdr;
#ifndef USE_PCAP_PER_OUT_PORT
        if( 0 == pcap_info.captured_packet_count) {
                onvm_util_get_cur_time(&pcap_info.capture_start_time);
        } else {
                if(pcap_info.captured_packet_count >= pcap_info.max_packet_count) return -1;
                if(pcap_info.captured_size >= pcap_info.max_capture_size) return -2;
                if(pcap_info.captured_packet_count >= pcap_info.max_packet_count || pcap_info.captured_size >= pcap_info.max_capture_size) {
                        onvm_util_clear_pcap_log(pcap_info.capture_port);
                        onvm_util_get_cur_time(&pcap_info.capture_start_time);
                }
        }
        gettimeofday(&pkt_hdr.ts, NULL);
        for (i = 0; i < nb_pkts; i++) {
                //ts_info[i];
                //pkt_hdr.ts.tv_sec = pkts[i]->ol_flags/1000000000;             // (pkts[i]->ol_flags = uint64_t (now.t.tv_sec * 1000000000 + now.t.tv_sec);
                //pkt_hdr.ts.tv_usec= ((pkts[i]->ol_flags -pkt_hdr.ts.tv_sec)*1000 ;
                pkt_hdr.caplen = rte_pktmbuf_data_len(pkts[i]);
                pkt_hdr.len = rte_pktmbuf_data_len(pkts[i]);
                packet = rte_pktmbuf_mtod(pkts[i], u_char * );
                pcap_dump((u_char *) pcap_info.pcap_dumper, &pkt_hdr, packet);
                cap_size += pkt_hdr.len;
        }
        pcap_info.captured_packet_count += nb_pkts;
        pcap_info.captured_size +=cap_size;
#else
        if( 0 == pcap_infos[port].captured_packet_count) {
                onvm_util_get_cur_time(&pcap_infos[port].capture_start_time);
        } else {
                if(pcap_infos[port].captured_packet_count >= pcap_infos[port].max_packet_count) return -1;
                if(pcap_infos[port].captured_size >= pcap_infos[port].max_capture_size) return -2;
                if(pcap_infos[port].captured_packet_count >= pcap_infos[port].max_packet_count || pcap_infos[port].captured_size >= pcap_infos[port].max_capture_size) {
                        onvm_util_clear_pcap_log(pcap_infos[port].capture_port);
                        onvm_util_get_cur_time(&pcap_infos[port].capture_start_time);
                }
        }
        gettimeofday(&pkt_hdr.ts, NULL);
        for (i = 0; i < nb_pkts; i++) {
                pkt_hdr.caplen = rte_pktmbuf_data_len(pkts[i]);
                pkt_hdr.len = rte_pktmbuf_data_len(pkts[i]);
                packet = rte_pktmbuf_mtod(pkts[i], u_char * );
                pcap_dump((u_char *) pcap_infos[port].pcap_dumper, &pkt_hdr, packet);
                cap_size += pkt_hdr.len;
        }
        pcap_infos[port].captured_packet_count += nb_pkts;
        pcap_infos[port].captured_size +=cap_size;
#endif
        return 0;
}

inline const char* onvm_util_close_and_get_pdump_file(__attribute__((unused)) int port) {
#ifndef USE_PCAP_PER_OUT_PORT
        if(pcap_info.is_active == pcap_log_status_unused)return NULL;

        else if(pcap_info.is_active==pcap_log_status_active_logging) {
                pcap_dump_close(pcap_info.pcap_dumper);
                pcap_close(pcap_info.pd);pcap_info.pd=NULL;
                pcap_info.is_active=pcap_log_status_active_logging_stopped;
        }

        if(pcap_info.captured_packet_count) return (const char*) pcap_info.capture_file;
#else
        if(pcap_infos[port].is_active == pcap_log_status_unused)return NULL;

        else if(pcap_infos[port].is_active==pcap_log_status_active_logging) {
                pcap_dump_close(pcap_infos[port].pcap_dumper);
                pcap_close(pcap_infos[port].pd);pcap_infos[port].pd=NULL;
                pcap_infos[port].is_active=pcap_log_status_active_logging_stopped;
        }

        if(pcap_infos[port].captured_packet_count) return (const char*) pcap_infos[port].capture_file;

#endif
        return NULL;
}
#else
inline int onvm_util_init_pacp_logger(__attribute__((unused)) int port, __attribute__((unused)) int mode) {
        uint8_t index = 0;
        for(index=0; index < MAX_PCAP_ROT_FILES; index++) {
                if(pcap_infos[port].log[index].pd) return EALREADY;

                pcap_infos[port].log[index].pd = pcap_open_dead_with_tstamp_precision(DLT_EN10MB, MAX_SNAPLEN, PCAP_TSTAMP_PRECISION_NANO);
                if(pcap_infos[port].log[index].pd == NULL) return 2;
                pcap_infos[port].log[index].capture_file = get_pcap_file_name(port,index);
                pcap_infos[port].log[index].pcap_dumper = pcap_dump_open(pcap_infos[port].log[index].pd, (const char*)pcap_infos[port].log[index].capture_file);
                if(NULL == pcap_infos[port].log[index].pcap_dumper) return 3;
                pcap_infos[port].log[index].captured_packet_count=0;
                pcap_infos[port].log[index].captured_size=0;
                pcap_infos[port].log[index].is_active=pcap_log_status_active_logging;


                pcap_infos[port].max_capture_size = PCAP_MAX_CAPTURE_SIZE;
                pcap_infos[port].max_packet_count = PCAP_MAX_PACKET_COUNT;
                pcap_infos[port].capture_port=port;
                pcap_infos[port].log_active_status = 0;
                pcap_infos[port].ring_index=0;
        }
        return 0;
}
inline int onvm_util_clear_pcap_log(__attribute__((unused)) int port) {
        uint8_t index = pcap_infos[port].ring_index;
        pcap_dump_close(pcap_infos[port].log[index].pcap_dumper);
        pcap_infos[port].log[index].pcap_dumper = NULL;
        pcap_infos[port].log[index].is_active = pcap_log_status_active_logging_stopped;
        pcap_close(pcap_infos[port].log[index].pd);
        index++;
        if(index >= MAX_PCAP_ROT_FILES) {
                index = 0;
        }
        pcap_infos[port].ring_index = index;

        if(NULL == pcap_infos[port].log[index].pcap_dumper) {
                pcap_infos[port].log[index].pd = pcap_open_dead_with_tstamp_precision(DLT_EN10MB, MAX_SNAPLEN, PCAP_TSTAMP_PRECISION_NANO);
                if(pcap_infos[port].log[index].pd == NULL) return 2;
                pcap_infos[port].log[index].pcap_dumper = pcap_dump_open(pcap_infos[port].log[index].pd, (const char*)pcap_infos[port].log[index].capture_file);
                if(NULL == pcap_infos[port].log[index].pcap_dumper) return 3;
                pcap_infos[port].log[index].captured_packet_count=0;
                pcap_infos[port].log[index].captured_size=0;
                pcap_infos[port].log[index].is_active=pcap_log_status_active_logging;
        }
        return 0;
}

inline int onvm_util_log_packets(__attribute__((unused)) int port, struct rte_mbuf **pkts, __attribute__((unused)) uint64_t *ts_info, uint16_t nb_pkts) {
        unsigned i, cap_size=0;
        u_char *packet;
        struct pcap_pkthdr pkt_hdr;

        if(!pcap_infos[port].log_active_status) pcap_infos[port].log_active_status=pcap_log_status_active_logging;

        uint8_t index = pcap_infos[port].ring_index;
        if(pcap_infos[port].log[index].captured_packet_count >= pcap_infos[port].max_packet_count || pcap_infos[port].log[index].captured_size >= pcap_infos[port].max_capture_size) {
                onvm_util_clear_pcap_log(pcap_infos[port].capture_port);
                index = pcap_infos[port].ring_index;
        }
        if( 0 == pcap_infos[port].log[index].captured_packet_count) {
                onvm_util_get_cur_time(&pcap_infos[port].log[index].capture_start_time);
                pcap_infos[port].log[index].is_active=pcap_log_status_active_logging;
        } else {
                //if(pcap_infos[port].log[index].captured_packet_count >= pcap_infos[port].max_packet_count) return -1;
                //if(pcap_infos[port].log[index].captured_size >= pcap_infos[port].max_capture_size) return -2;
        }

        gettimeofday(&pkt_hdr.ts, NULL);
        for (i = 0; i < nb_pkts; i++) {
                pkt_hdr.caplen = rte_pktmbuf_data_len(pkts[i]);
                pkt_hdr.len = rte_pktmbuf_data_len(pkts[i]);
                packet = rte_pktmbuf_mtod(pkts[i], u_char * );
                pcap_dump((u_char *) pcap_infos[port].log[index].pcap_dumper, &pkt_hdr, packet);
                cap_size += pkt_hdr.len;
        }
        pcap_infos[port].log[index].captured_packet_count += nb_pkts;
        pcap_infos[port].log[index].captured_size +=cap_size;
        return 0;
}
inline const char* onvm_util_close_and_get_pdump_file(__attribute__((unused)) int port) {
        uint8_t index = pcap_infos[port].ring_index;
        if(pcap_infos[port].log_active_status == pcap_log_status_unused)return NULL;
        else if (pcap_log_status_active_logging==pcap_infos[port].log_active_status) {
                pcap_dump_close(pcap_infos[port].log[index].pcap_dumper);
                pcap_close(pcap_infos[port].log[index].pd);pcap_infos[port].log[index].pd=NULL;
                pcap_infos[port].log[index].is_active=pcap_log_status_active_logging_stopped;
        }
        pcap_infos[port].log_active_status=pcap_log_status_active_logging_stopped;
        if(pcap_infos[port].log[index].captured_packet_count) return (const char*) pcap_infos[port].log[index].capture_file;
        return NULL;
}
#endif


static inline int onvm_util_plain_pcap_replay(uint8_t port, uint64_t max_duration_us);
inline int onvm_util_replay_all_packets(uint8_t port, uint64_t max_duration_us) {
        onvm_util_close_and_get_pdump_file(port);

#ifdef USE_FREAD_MODE
        return onvm_util_file_pcap_replay(port,max_duration_us);
#else
        return onvm_util_plain_pcap_replay(port,max_duration_us);
#endif

}
#ifndef USE_PCAP_ROTATION_BUFFERS
static inline int onvm_util_plain_pcap_replay(uint8_t port, uint64_t max_duration_us) {
        int ret = 0;
        uint16_t rp_count;
        struct rte_mbuf *pkts[PACKET_READ_SIZE];
        struct pcap_pkthdr *hdr;
        //struct timeval now;
        //struct ipv4_hdr * ip_h;
        void * pkt;
        char err_buf[256];
        int continue_replay=1;
        uint64_t replay_pkt_count=0;

#ifndef USE_PCAP_PER_OUT_PORT
        pcap_info.pd = pcap_open_offline(pcap_info.capture_file, err_buf);
        if(pcap_info.pd == NULL) {
                printf("\n Unable to open PCAP Capture File=%s\n", pcap_info.capture_file);
                return 2;
        }
        pcap_info.is_active=pcap_log_status_replay_on;
        do {
                ret = pcap_next_ex(pcap_info.pd, &hdr, (const u_char**)&pkt);
                if(ret <=0) {
                        continue_replay = 0;
                        break;
                }
                pkts[rp_count] = rte_pktmbuf_alloc(state_info->pktmbuf_pool);
                if(NULL == pkts[rp_count]) {
                        break;
                }
                pkts[rp_count]->data_len = pkts[rp_count]->pkt_len = hdr->caplen;
                rte_memcpy ( (char*) pkts[rp_count]->buf_addr + pkts[rp_count]->data_off, pkt, hdr->caplen);
                rp_count++;
                if(rp_count == PACKET_READ_SIZE) {
                        int enq_status = rte_ring_enqueue_bulk(nf0_cl->tx_q, (void **)pkts, PACKET_READ_SIZE,NULL);
                        if( 0 == enq_status) { //if (-ENOBUFS == enq_status) {
                                onvm_pkt_drop_batch(pkts,rp_count);
                                nf0_cl->stats.rx_drop += rp_count;
                        }
                        replay_pkt_count+=rp_count;
                        rp_count=0;
                }
                //Add check for elapsed time and break appropriately
        }while(continue_replay);
        if(rp_count == PACKET_READ_SIZE) {
                int enq_status = rte_ring_enqueue_bulk(nf0_cl->tx_q, (void **)pkts, PACKET_READ_SIZE,NULL);
                if(0 == enq_status) { //if (-ENOBUFS == enq_status) {
                        onvm_pkt_drop_batch(pkts,rp_count);
                        nf0_cl->stats.rx_drop += rp_count;
                }
                replay_pkt_count+=rp_count;
                rp_count=0;
        }
        printf("\n [%lld] Replay packets sent on port [%d] within [%lld]!\n", (long long)replay_pkt_count, port, (long long)max_duration_us);
        pcap_close(pcap_info.pd);
        pcap_info.is_active=pcap_log_status_replay_finished;
#else
        pcap_infos[port].pd = pcap_open_offline(pcap_infos[port].capture_file, err_buf);
        if(pcap_infos[port].pd == NULL) {
                printf("\n Unable to open PCAP Capture File=%s\n", pcap_infos[port].capture_file);
                return 2;
        }
        pcap_infos[port].is_active=pcap_log_status_replay_on;
        do {
                ret = pcap_next_ex(pcap_infos[port].pd, &hdr, (const u_char**)&pkt);
                if(ret <=0) {
                        continue_replay = 0;
                        break;
                }
                pkts[rp_count] = rte_pktmbuf_alloc(state_info->pktmbuf_pool);
                if(NULL == pkts[rp_count]) {
                        break;
                }
                pkts[rp_count]->data_len = pkts[rp_count]->pkt_len = hdr->caplen;
                rte_memcpy ( (char*) pkts[rp_count]->buf_addr + pkts[rp_count]->data_off, pkt, hdr->caplen);
                rp_count++;
                if(rp_count == PACKET_READ_SIZE) {
                        int enq_status = rte_ring_enqueue_bulk(nf0_cl->tx_q, (void **)pkts, PACKET_READ_SIZE,NULL);
                        if(0 == enq_status) { //if (-ENOBUFS == enq_status) {
                                onvm_pkt_drop_batch(pkts,rp_count);
                                nf0_cl->stats.rx_drop += rp_count;
                        }
                        replay_pkt_count+=rp_count;
                        rp_count=0;
                }
                //Add check for elapsed time and break appropriately
        }while(continue_replay);
        if(rp_count == PACKET_READ_SIZE) {
                int enq_status = rte_ring_enqueue_bulk(nf0_cl->tx_q, (void **)pkts, PACKET_READ_SIZE,NULL);
                if(0 == enq_status) { //if (-ENOBUFS == enq_status) {
                        onvm_pkt_drop_batch(pkts,rp_count);
                        nf0_cl->stats.rx_drop += rp_count;
                }
                replay_pkt_count+=rp_count;
                rp_count=0;
        }
        printf("\n [%lld] Replay packets sent on port [%d] within [%lld]!\n", (long long)replay_pkt_count, port, (long long)max_duration_us);
        pcap_close(pcap_infos[port].pd);
        pcap_infos[port].is_active=pcap_log_status_replay_finished;
#endif
        return ret;
}
#else
static inline int onvm_util_plain_pcap_replay(uint8_t port, uint64_t max_duration_us) {
        int ret = 0;
        uint16_t rp_count;
        struct rte_mbuf *pkts[PACKET_READ_SIZE];
        struct pcap_pkthdr *hdr;
        //struct timeval now;
        //struct ipv4_hdr * ip_h;
        void * pkt;
        char err_buf[256];
        int continue_replay=1;
        uint64_t replay_pkt_count=0;
        uint8_t index = pcap_infos[port].ring_index;
#define ENABLE_LOCAL_LATENCY_PROFILER
#ifdef ENABLE_LOCAL_LATENCY_PROFILER
        onvm_interval_timer_t ts;
        onvm_util_get_start_time(&ts);
#endif


        pcap_infos[port].log[index].pd = pcap_open_offline(pcap_infos[port].log[index].capture_file, err_buf);
        if(pcap_infos[port].log[index].pd == NULL) {
                printf("\n Unable to open PCAP Capture File=%s, err:%s\n", pcap_infos[port].log[index].capture_file, err_buf);
                return 2;
        }
        pcap_infos[port].log_active_status=pcap_log_status_replay_on;
        do {
                ret = pcap_next_ex(pcap_infos[port].log[index].pd, &hdr, (const u_char**)&pkt);
                if(ret <=0) {
                        continue_replay = 0;
                        break;
                }
                pkts[rp_count] = rte_pktmbuf_alloc(state_info->pktmbuf_pool);
                if(NULL == pkts[rp_count]) {
                        break;
                }
                pkts[rp_count]->data_len = pkts[rp_count]->pkt_len = hdr->caplen;
                rte_memcpy ( (char*) pkts[rp_count]->buf_addr + pkts[rp_count]->data_off, pkt, hdr->caplen);
                rp_count++;
                if(rp_count == PACKET_READ_SIZE) {
                        int enq_status = rte_ring_enqueue_bulk(nf0_cl->tx_q, (void **)pkts, PACKET_READ_SIZE,NULL);
                        if (0 == enq_status) {
                                onvm_pkt_drop_batch(pkts,rp_count);
                                nf0_cl->stats.rx_drop += rp_count;
                        }
                        replay_pkt_count+=rp_count;
                        rp_count=0;
                }
                //Add check for elapsed time and break appropriately
        }while(continue_replay);
        if(rp_count) {
                int enq_status = rte_ring_enqueue_bulk(nf0_cl->tx_q, (void **)pkts, rp_count,NULL);
                if (0 == enq_status) {
                        onvm_pkt_drop_batch(pkts,rp_count);
                        nf0_cl->stats.rx_drop += rp_count;
                }
                replay_pkt_count+=rp_count;
                rp_count=0;
        }
#ifdef ENABLE_LOCAL_LATENCY_PROFILER
        int64_t ttl_elapsed = onvm_util_get_elapsed_time(&ts);
        printf("REPLAY START-->REPLAY END): %li ns\n", ttl_elapsed);
#endif
        printf("\n [%lld] Replay packets sent on port [%d] within [%lld]!\n", (long long)replay_pkt_count, port, (long long)max_duration_us);
        pcap_close(pcap_infos[port].log[index].pd);
        pcap_infos[port].log_active_status=pcap_log_status_replay_finished;
        return ret;
}
#endif
/*********************** PCAP DUMP FEATURE ************************************/
#endif //ENABLE_PCAP_CAPTURE
/******************************************************************************/
/*******************************File Interface functions********************************/
int onv_pkt_send_to_special_nf0(__attribute__((unused)) struct thread_info *rx, struct rte_mbuf *pkts[], uint16_t rx_count) {

        /* Note: This direct_call to onv_pkt_send_on_alt_port() here results in 14.88Mpps;
         * while: (when No other nfs registered) direct send to Rx Ring and Then process by Main thread to push packets out is resulting in 13.1Mpps.
         * and when some nfs are registered, packets are processed through default chain and directed here to send on Rx ring and then processed by main thread to push packets out results in 9.2 to 10Mpps.
         *
         * case 1: NIC --> Rx Thread --> NIC
         * Configuration: Rx Thread --> onv_pkt_send_to_special_nf0() with SEND_DIRECT_ON_ALT_PORT enabled and code in onv_pkt_send_to_special_nf0() = onv_pkt_send_on_alt_port();
         * Throughput: 14.88Mpps
         * case 2: NIC --> Rx Thread --> NF0 Tx Ring --> Tx Thread --> NIC
         * Configuration: Rx --> onv_pkt_send_to_special_nf0() with SEND_DIRECT_ON_ALT_PORT disabled and code in onv_pkt_send_to_special_nf0() = onv_pkt_send_on_alt_port();
         * Throughput: 14.88Mpps
         * case 3:  NIC --> Rx Thread (direct) --> NF0 Rx Ring --> Main Thread --> NF0 Tx Ring --> Tx Thread --> NIC
         * Configuration: Rx --> onv_pkt_send_to_special_nf0() and code in and code in onv_pkt_send_to_special_nf0() = rte_ring_enqueue_bulk(rx_ing)
         * Throughput: 13.0 to 13.3Mpps
         * case 4: NIC --> Rx Thread (FT Query, def chain) --> NF0 Rx Ring --> Main Thread --> NF0 Tx Ring --> Tx Thread --> NIC
         * Configuration: Rx --> onvm_pkt_process_rx_batch() and code in and code in onv_pkt_send_to_special_nf0() = rte_ring_enqueue_bulk(rx_ing)
         * Throughput: 9.1Mpps and 10.1~10.5Mpps
         */

#ifdef ONVM_MGR_ACT_AS_2PORT_FWD_BRIDGE
        return onv_pkt_send_on_alt_port(rx,pkts,rx_count);
#else
        //onvm_pkt_drop_batch(pkts, rx_count);
        //return 0;
#endif //ONVM_MGR_ACT_AS_2PORT_FWD_BRIDGE

        //if(NULL == nf0_cl) nf0_cl = &nfs[0];
        /* Check if NF is valid */
        if (!onvm_nf_is_valid(nf0_cl)) {
                onvm_pkt_drop_batch(pkts, rx_count);
                return 0;
        }

        int enq_status = rte_ring_enqueue_bulk(nf0_cl->rx_q, (void **)pkts,rx_count,NULL);
        /* Update statistics of inserted/dropped packets */
        if ( 0 == enq_status) {
                uint16_t i;
                for (i = 0; i < rx_count; i++) {
                        onvm_pkt_drop(pkts[i]);
                }
                nf0_cl->stats.rx_drop += rx_count;
        }
        else {
                nf0_cl->stats.rx += rx_count;
        }
        return 0;
}

int process_special_nf0_rx_packets(void) {

        struct rte_mbuf *pkts[PACKET_READ_SIZE];
        struct onvm_pkt_meta* meta = NULL;
#ifdef ENABLE_VXLAN
uint16_t nic_port = DISTRIBUTED_NIC_PORT;
        int ret;
#ifdef ENABLE_ZOOKEEPER
        struct ether_addr dst_addr;
        uint16_t dst_service_id;
        int64_t remote_id;
#endif
#endif

        //if(NULL == nf0_cl) nf0_cl = &nfs[0];
        /* Check if NF is valid */
        if (!onvm_nf_is_valid(nf0_cl)) {
                return 0;
        }

        for (; keep_running;) {
                uint16_t nb_pkts = PACKET_READ_SIZE;

                nb_pkts = (uint16_t)rte_ring_dequeue_burst(nf0_cl->rx_q, (void**)pkts, nb_pkts,NULL);
                if(nb_pkts == 0) {
                        return 0;
                }
                /* Give each packet to the specific processing function : Based on ETH_TYPE and Registered MGR Services */
                uint32_t i = 0;
                for (; i < nb_pkts; i++) {
                        struct ether_hdr *eth = rte_pktmbuf_mtod(pkts[i], struct ether_hdr *);
                        meta = onvm_get_pkt_meta((struct rte_mbuf*)pkts[i]);
                        if(ONVM_NF_ACTION_DROP == meta->action) {
                                onvm_pkt_drop(pkts[i]); continue;
                        }
                        switch(rte_be_to_cpu_16(eth->ether_type)) {
                        default:
                        case ETHER_TYPE_IPv4:
#ifdef ENABLE_VXLAN
                                /* Encapsulate vxlan pkt */
                                printf("before encap\n");
                                rte_pktmbuf_dump(stdout, pkts[i], pkts[i]->pkt_len);

#ifdef ENABLE_ZOOKEEPER
                                meta = onvm_get_pkt_meta((struct rte_mbuf*)pkts[i]);
                                dst_service_id = meta->destination;
                                remote_id = onvm_zk_lookup_service(pkts[i], dst_service_id, &dst_addr);
                                if (remote_id != 0) onvm_encapsulate_pkt(pkts[i], &ports->mac[nic_port], &dst_addr);
                                //onvm_pkt_enqueue_port(NULL, nic_port, pkts[i]);
#else
                                onvm_encapsulate_pkt(pkts[i], &ports->mac[nic_port], &remote_eth_addr_struct);
#endif
                                printf("after encap\n");
                                rte_pktmbuf_dump(stdout, pkts[i], pkts[i]->pkt_len);

                                /* Decapsulate vxlan pkt */
                                ret = onvm_decapsulate_pkt(pkts[i]);
                                if (ret == -1) {
                                    meta = onvm_get_pkt_meta((struct rte_mbuf*)pkts[i]);
                                    onvm_ft_handle_packet(pkts[i], meta);
                                }
                                printf("after decap\n");
                                rte_pktmbuf_dump(stdout, pkts[i], pkts[i]->pkt_len);
                                onvm_pkt_drop(pkts[i]);
                                //rte_ring_enqueue(nf0_cl->tx_q, (void *)pkts[i]);
#else
                                meta = onvm_get_pkt_meta((struct rte_mbuf*)pkts[i]);
                                if(0 == onvm_ft_handle_packet(pkts[i], meta)) {
                                        //if (-ENOBUFS == rte_ring_enqueue_bulk(nf0_cl->tx_q, (void **)&pkts[i], 1)) {
                                        if (0 == rte_ring_enqueue_bulk(nf0_cl->tx_q, (void **)&pkts[i], 1,NULL)) {
                                                onvm_pkt_drop_batch(&pkts[i],1);
                                                nf0_cl->stats.rx_drop += 1;
                                        }
                                } else {
                                        onvm_pkt_drop(pkts[i]);
                                }
#endif
                                break;
                        case ETHER_TYPE_ARP:
                                if(try_check_and_send_arp_response(pkts[i],onvm_get_pkt_meta((struct rte_mbuf*)pkts[i]))) {
                                        /* For now Only service is INTERNAL_BRIDGE */
#ifdef ONVM_MGR_ACT_AS_2PORT_FWD_BRIDGE
                                        onv_pkt_send_on_alt_port(NULL,&pkts[i],1);
#else
                                        onvm_pkt_drop_batch(&pkts[i], 1);
#endif //ONVM_MGR_ACT_AS_2PORT_FWD_BRIDGE
                                }
                                break;
                        case ETHER_TYPE_RARP:
                                /* For now Only service is INTERNAL_BRIDGE */
#ifdef ONVM_MGR_ACT_AS_2PORT_FWD_BRIDGE
                                onv_pkt_send_on_alt_port(NULL,&pkts[i],1);
#else
                                        onvm_pkt_drop_batch(&pkts[i], 1);
#endif //ONVM_MGR_ACT_AS_2PORT_FWD_BRIDGE
                                break;
                        case ETHER_TYPE_RSYNC_DATA:
                                /* For now Only service is INTERNAL_BRIDGE */
#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
                                rsync_process_rsync_in_pkts(NULL,&pkts[i],1);
#endif //ONVM_MGR_ACT_AS_2PORT_FWD_BRIDGE
                                rte_pktmbuf_free(pkts[i]);//onvm_pkt_drop_batch(&pkts[i], 1);
                                break;
                        case ETHER_TYPE_BFD:
#ifdef ENABLE_BFD
                                onvm_bfd_process_incoming_packets(NULL,&pkts[i],1);
#endif
                                rte_pktmbuf_free(pkts[i]);//onvm_pkt_drop_batch(&pkts[i], 1);
                                break;

                        }
                }
#if 0

#endif
        }
        return 0;
}
int start_special_nf0(void) {
        //Make use of the internal NF[0]

        if(NULL == nf0_cl) nf0_cl = &nfs[0];
        // DO ONCE: Ensure destination NF is running and ready to receive packets
        if (!onvm_nf_is_valid(nf0_cl)) {
                void *mempool_data = NULL;
                struct onvm_nf_info *info = NULL;
                struct rte_mempool *nf_info_mp = NULL;
                nf_info_mp = rte_mempool_lookup(_NF_MEMPOOL_NAME);
                if (nf_info_mp == NULL) {
                        printf("Failed to get NF_MEMPOOL");
                        return 0;
                }
                if (rte_mempool_get(nf_info_mp, &mempool_data) < 0) {
                        printf("Failed to get client info memory");
                        return 0;
                }
                if (mempool_data == NULL) {
                        printf("Client Info struct not allocated");
                        return 0;
                }

                info = (struct onvm_nf_info*) mempool_data;
                info->instance_id = ONVM_SPECIAL_NF_SERVICE_ID;
                info->service_id = ONVM_SPECIAL_NF_INSTANCE_ID;
                info->tag = "SPECIAL_NF0"; //"INTERNAL_BRIDGE";
                info->status = NF_STARTING;
                nf0_cl->info=info;
                onvm_nf_register_run(info);
                //info->status = NF_RUNNING;

                /* Add all services of Special NF: IDeally Register services from callback */
                onvm_special_nf_arp_responder_init();
                init_onvm_ft_install();

#ifdef ENABLE_PCAP_CAPTURE
#ifndef USE_PCAP_PER_OUT_PORT
                onvm_util_init_pacp_logger(0,0);
#else
                uint8_t i = 0;
                for(i=0; i<ports->num_ports; i++) {
                        onvm_util_init_pacp_logger(ports->id[i],0);
                }
#endif
#endif
        }

        return onvm_nf_is_valid(nf0_cl);
}

int stop_special_nf0(void) {
        return 0;
}
