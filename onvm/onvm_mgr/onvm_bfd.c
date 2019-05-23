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

                              onvm_bfd.c

       This file contains all functions related to BFD management.

******************************************************************************/
#include "onvm_bfd.h"
#include "onvm_mgr.h"
#include "onvm_pkt.h"

#ifndef BFD_TX_PORT_QUEUE_ID
#define BFD_TX_PORT_QUEUE_ID (0)
#endif

//#include <rte_mbuf.h>
/********************* BFD Specific Defines and Structs ***********************/
#define BFD_PKT_OFFSET (sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr))

typedef struct bfd_session_status {
        //Active or passive mode: currently all active
        uint8_t mode;

        //State tacking and last known diagnostic messages from the peers
        BFD_StateValue local_state;
        BFD_StateValue remote_state;
        BFD_DiagStateValue local_diags;
        BFD_DiagStateValue remote_diags;

        //Connection descriptors: local node and remote node
        uint64_t local_descr;
        uint64_t remote_descr;

        //Negotiated interval for Tx/Tx communication: currently fixed and not using this field
        uint64_t tx_rx_interval;
        //TS of last sent packet from local port
        uint64_t last_sent_pkt_ts;
        //TS of last received packet from remote port
        uint64_t last_rcvd_pkt_ts;
        //Counter to track missed packets. currently used to slow rate send; as single composite timeout is used.
        uint64_t pkt_missed_counter;
        //Link Status change counter;
        uint64_t bfd_status_change_counter;

#ifdef PIGGYBACK_BFD_ON_ACTIVE_PORT_TRAFFIC
        //measure of last batch of rx packets on this port
        uint64_t last_rx_pkts;
        uint8_t last_rx_set;
        uint8_t  skip_bfd_query;
#endif

}bfd_session_status_t;

typedef struct bfd_pkt_stat_t {
        uint64_t rx_count;
        uint64_t tx_count;
}bfd_pkt_stat_t;

bfd_pkt_stat_t bfd_stat;

extern struct rte_mempool *pktmbuf_pool;
struct rte_timer bfd_status_checkpoint_timer;
bfd_session_status_t bfd_sess_info[RTE_MAX_ETHPORTS];
bfd_status_notifier_cb notifier_cb;
BfdPacket bfd_template;
/********************* BFD Specific Defines and Structs ***********************/

/********************* Local Functions Declaration ****************************/
struct rte_mbuf* create_bfd_packet(void);
int parse_bfd_packet(struct rte_mbuf* pkt);
static void send_bfd_echo_packets(void);
static void check_bdf_remote_status(void);

struct rte_mbuf* create_bfd_packet_spcl(BfdPacketHeader *pkt_hdr);
void send_bfd_echo_packets_spcl(BfdPacketHeader *pkt_hdr);
int print_bfd_packet(BfdPacket *bfdp);
//#define BFD_PROFILE_RTT
/********************** Local Functions Definition ****************************/
static void init_bfd_session_status(onvm_bfd_init_config_t *bfd_config) {
        uint8_t i = 0;
        for(i=0;i< ports->num_ports;i++) {
                bfd_sess_info[i].mode           = bfd_config->session_mode[i];
                bfd_sess_info[i].local_state    = Init;
                bfd_sess_info[i].remote_state   = Init;

                bfd_sess_info[i].local_diags    = None;
                bfd_sess_info[i].remote_diags   = None;

                bfd_sess_info[i].local_descr    = bfd_config->bfd_identifier;
                bfd_sess_info[i].remote_descr   = bfd_config->bfd_identifier;

                bfd_sess_info[i].tx_rx_interval = 0;
                bfd_sess_info[i].last_sent_pkt_ts = 0;
                bfd_sess_info[i].last_rcvd_pkt_ts = 0;
                bfd_sess_info[i].pkt_missed_counter = 0;

#ifdef PIGGYBACK_BFD_ON_ACTIVE_PORT_TRAFFIC
                bfd_sess_info[i].last_rx_pkts = 0;
                bfd_sess_info[i].last_rx_set = 0;
                bfd_sess_info[i].skip_bfd_query= 0;
#endif
        }
}
int
onvm_bfd_start(void) {
        return 0;
}

int
onvm_bfd_stop(void) {
        return 0;
}
static void
bfd_status_checkpoint_timer_cb(__attribute__((unused)) struct rte_timer *ptr_timer,
        __attribute__((unused)) void *ptr_data) {
//#define __DEBUG_BFD_LOGS__
# ifdef __DEBUG_BFD_LOGS__
        static uint64_t cur_cycles = 0; //onvm_util_get_current_cpu_cycles();
        static uint64_t prev_cycles = 0;
        cur_cycles = onvm_util_get_current_cpu_cycles();
        if(prev_cycles) {printf("In bfd_status_checkpoint_timer_cb@: %"PRIu64"\n", onvm_util_get_diff_cpu_cycles_in_us(prev_cycles, cur_cycles)  );}  prev_cycles=cur_cycles;
        //printf("In bfd_status_checkpoint_timer_cb@: %"PRIu64"\n", onvm_util_get_current_cpu_cycles() );
#endif
        check_bdf_remote_status();
        send_bfd_echo_packets();
        return;
}
static inline int initialize_bfd_timers(void) {
        uint64_t ticks = ((uint64_t)BFD_CHECKPOINT_PERIOD_IN_US *(rte_get_timer_hz()/1000000));
        rte_timer_reset_sync(&bfd_status_checkpoint_timer,ticks,PERIODICAL,
                        rte_lcore_id(), &bfd_status_checkpoint_timer_cb, NULL);
        return 0;
}
/******************** BFD Packet Processing Functions *************************/
static inline int bfd_send_packet_out(uint8_t port_id, uint16_t queue_id, struct rte_mbuf *tx_pkt) {
        uint16_t sent_packets = rte_eth_tx_burst(port_id,queue_id, &tx_pkt, 1);
        if(unlikely(sent_packets  == 0)) {
                onvm_pkt_drop(tx_pkt);
        }
        //printf("\n %d BFD Packets were sent!", sent_packets);
        return sent_packets;
}
static void set_bfd_packet_template(uint32_t my_desc) {
        bfd_template.header.versAndDiag = 0x00;
        bfd_template.header.flags= 0x00;
        bfd_template.header.length=sizeof(BfdPacket);
        bfd_template.header.myDisc=rte_cpu_to_be_32(my_desc);
        bfd_template.header.yourDisc=0;
        bfd_template.header.txDesiredMinInt=rte_cpu_to_be_32(BFDMinTxInterval_us);
        bfd_template.header.rxRequiredMinInt=rte_cpu_to_be_32(BFDMinRxInterval_us);
        bfd_template.header.rxRequiredMinEchoInt=rte_cpu_to_be_32(BFDEchoInterval_us);
}
int print_bfd_packet(BfdPacket *bfdp) {
        printf("VersAndDiag: %" PRIu8 "\n", bfdp->header.versAndDiag & 0b11111111);
        printf("FLAGS: %" PRIu8 "\n", bfdp->header.flags & 0b11111111);
        printf("Length: %" PRIu8 "\n", bfdp->header.length & 0b11111111);
        printf("myDisc: %" PRIu32 "\n", rte_be_to_cpu_32(bfdp->header.myDisc));
        printf("YrDisc: %" PRIu32 "\n", rte_be_to_cpu_32(bfdp->header.yourDisc));
        printf("txDesiredMinInt: %" PRIu32 "\n", rte_be_to_cpu_32(bfdp->header.txDesiredMinInt));
        printf("rxRequiredMinInt: %" PRIu32 "\n", rte_be_to_cpu_32(bfdp->header.rxRequiredMinInt));
        printf("rxRequiredMinEchoInt: %" PRIu32 "\n", rte_be_to_cpu_32(bfdp->header.rxRequiredMinEchoInt));
        return bfdp->header.flags ;
}
static void parse_and_set_bfd_session_info(struct rte_mbuf* pkt,BfdPacket *bfdp) {
        uint8_t port_id = pkt->port; //rem_desc & 0xFF;
        if(port_id < ports->num_ports) {
                if(Init == bfd_sess_info[port_id].remote_state) {
                        bfd_sess_info[port_id].remote_state = Up;
                        bfd_sess_info[port_id].remote_descr = rte_be_to_cpu_32(bfdp->header.myDisc);
                } else if ((Down == bfd_sess_info[port_id].remote_state) || (AdminDown == bfd_sess_info[port_id].remote_state)) {
                        printf("BFD in Down or Admin Down State:%d moved to up!\n", bfd_sess_info[port_id].remote_state);
                        bfd_sess_info[port_id].remote_state = Up;
                        bfd_sess_info[port_id].remote_descr = rte_be_to_cpu_32(bfdp->header.myDisc);
                }else if (Up == bfd_sess_info[port_id].remote_state) {
                } else {
                        printf("BFD in Unknown State:%d\n", bfd_sess_info[port_id].remote_state);
                }
                //bfd_sess_info[port_id].remote_state = (BFD_StateValue)bfdp->header.flags;   //todo: parse flag to status
                bfd_sess_info[port_id].remote_diags = (BFD_DiagStateValue)bfdp->header.versAndDiag; //todo: parse verse_and_diag to diag_status
                bfd_sess_info[port_id].last_rcvd_pkt_ts = onvm_util_get_current_cpu_cycles();

#ifdef BFD_PROFILE_RTT
                print_bfd_packet(bfdp);
                if(bfdp->header.flags == 1) {
                        send_bfd_echo_packets_spcl(&bfdp->header);
                } else if(bfdp->header.flags == 2){
                        //uint64_t s_time = (bfdp->header.myDisc | ((bfdp->header.yourDisc)<<32));
                        uint64_t s_time = rte_be_to_cpu_32(bfdp->header.yourDisc);
                        s_time <<=32;
                        s_time |=rte_be_to_cpu_32(bfdp->header.myDisc);
                        printf("Received Response Message: after a Delay of: %lld\n", (long long int) onvm_util_get_elapsed_cpu_cycles_in_us(s_time));
                }
#endif
        }
}

struct rte_mbuf* create_bfd_packet(void) {
        //printf("\n Crafting BFD packet for buffer [%p]\n", pkt);

        if(NULL == pktmbuf_pool) {
                return NULL;
        }
        struct rte_mbuf* pkt = rte_pktmbuf_alloc(pktmbuf_pool);
        if(NULL == pkt) {
                printf("\n Failed to Get Packet from pktmbuf_pool!\n");
                return NULL;
        }

        /* craft eth header */
        struct ether_hdr *ehdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
        /* set ether_hdr fields here e.g. */
        memset(ehdr,0, sizeof(struct ether_hdr));
        //memset(&ehdr->s_addr,0, sizeof(ehdr->s_addr));
        //memset(&ehdr->d_addr,0, sizeof(ehdr->d_addr));
        //ehdr->ether_type = rte_bswap16(ETHER_TYPE_IPv4);
        ehdr->ether_type = rte_bswap16(ETHER_TYPE_BFD);     //change to specific type for ease of packet handling.

        /* craft ipv4 header */
        struct ipv4_hdr *iphdr = (struct ipv4_hdr *)(&ehdr[1]);
        memset(iphdr,0, sizeof(struct ipv4_hdr));

        /* set ipv4 header fields here */
        struct udp_hdr *uhdr = (struct udp_hdr *)(&iphdr[1]);
        /* set udp header fields here, e.g. */
        uhdr->src_port = rte_bswap16(3784);
        uhdr->dst_port = rte_bswap16(3784);
        uhdr->dgram_len = sizeof(BfdPacket);

        BfdPacket *bfdp = (BfdPacket *)(&uhdr[1]);
        memset(bfdp,0,sizeof(BfdPacket));
        bfdp->header.flags = 0;

        rte_memcpy(bfdp, &bfd_template, sizeof(BfdPacket));

#ifdef BFD_PROFILE_RTT
        uint64_t tm = onvm_util_get_current_cpu_cycles();
        bfdp->header.myDisc = rte_cpu_to_be_32(tm & 0xFFFFFFFF);
        bfdp->header.yourDisc     = rte_cpu_to_be_32(tm >> 32);
        bfdp->header.flags       =1;
#endif
        //printf("Sending BFD PAcket with following Information\n");
        //print_bfd_packet(bfdp);

        //set packet properties
        size_t pkt_size = sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr) + sizeof(BfdPacket);
        pkt->data_len = pkt_size;
        pkt->pkt_len = pkt_size;

        return pkt;
}
int parse_bfd_packet(struct rte_mbuf* pkt) {
        struct udp_hdr *uhdr;
        BfdPacket *bfdp = NULL;

        uhdr = (struct udp_hdr*)(rte_pktmbuf_mtod(pkt, uint8_t*) + sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr));
        //bfdp = (BfdPacket*)(rte_pktmbuf_mtod(pkt, uint8_t*) + BFD_PKT_OFFSET);
        bfdp = (BfdPacket *)(&uhdr[1]);

        if(unlikely((sizeof(BfdPacket) > rte_be_to_cpu_16(uhdr->dgram_len)))) {
                printf("Size of BfdPacket[%d], size of UDP Header Data Len:%d\n", (int) sizeof(BfdPacket), (int) rte_be_to_cpu_16(uhdr->dgram_len));
                return -1;
        }

        //printf("\n Parse BFD packet\n");
        parse_and_set_bfd_session_info(pkt, bfdp);
        //if(bfdp->header.flags == 0) return 0;
        return 0;
}
static void send_bfd_echo_packets(void) {
        uint16_t i=0;
        struct rte_mbuf *pkt = NULL;
        for(i=0; i< ports->num_ports; i++) {
                if(Init == bfd_sess_info[i].local_state) {
                        bfd_sess_info[i].local_state = Up;
                }else if (Down == bfd_sess_info[i].local_state) {
                        bfd_sess_info[i].local_state = Up; //bypass handshake protocol just start with run.
                }else if (AdminDown == bfd_sess_info[i].local_state) continue;

#ifdef PIGGYBACK_BFD_ON_ACTIVE_PORT_TRAFFIC
                if(bfd_sess_info[i].skip_bfd_query) continue;
#endif
                //Avoid sending to the node that is explicitly marked as Down i.e. Admin down
                if (AdminDown == bfd_sess_info[i].remote_state) continue;
                //Else probe at much lower frequency
                else if (Init == bfd_sess_info[i].remote_state || Down == bfd_sess_info[i].remote_state ) {
                        if(BFD_SLOW_SEND_RATE_RATIO_COUNTER > bfd_sess_info[i].pkt_missed_counter++) continue;
                        bfd_sess_info[i].pkt_missed_counter = 0;    //Further, every switch to up should clear this counter. or it is just fine if not reset.
                }

                pkt = create_bfd_packet();
                if(pkt) {
                        bfd_sess_info[i].last_sent_pkt_ts = onvm_util_get_current_cpu_cycles();
                        bfd_send_packet_out(i, BFD_TX_PORT_QUEUE_ID, pkt);
                        bfd_stat.tx_count+=1;
                }
        }
        return ;
}
static void check_bdf_remote_status(void) {
        uint16_t i=0;
        uint64_t elapsed_time = 0;
        for(i=0; i< ports->num_ports; i++) {
                if(ports->down_status[i]) continue;
                if(bfd_sess_info[i].remote_state !=Up || (bfd_sess_info[i].mode == BFD_SESSION_MODE_PASSIVE)) {
#ifdef PIGGYBACK_BFD_ON_ACTIVE_PORT_TRAFFIC
                        bfd_sess_info[i].last_rx_set=0;
#endif
                        continue;
                }

#ifdef PIGGYBACK_BFD_ON_ACTIVE_PORT_TRAFFIC
                uint64_t rx_pkts = ports->rx_stats.rx[i];
                if(rx_pkts && (0 == bfd_sess_info[i].last_rx_set)){
                        bfd_sess_info[i].last_rx_set=1;
                        bfd_sess_info[i].last_rx_pkts = rx_pkts;
                } else {
                        //bfd_sess_info[i].last_rx_pkts = rx_pkts;// - bfd_sess_info[i].last_rx_pkts;
                }
                //Check to ensure that only BFD Packets are ignored! otherwise, every BFD packet also causes to stall resulting in timeout after every BFD packet!!
                if((rx_pkts - bfd_sess_info[i].last_rx_pkts) > BFD_MAX_PER_INTV_PER_PORT) {//if((rx_pkts - bfd_sess_info[i].last_rx_pkts)) {//
                        bfd_sess_info[i].skip_bfd_query= 1;
                        bfd_sess_info[i].last_rcvd_pkt_ts = onvm_util_get_current_cpu_cycles();
                        bfd_sess_info[i].last_rx_pkts = rx_pkts;
                        //continue;
                }
                else {
                        bfd_sess_info[i].last_rx_pkts = rx_pkts;
                        if(bfd_sess_info[i].skip_bfd_query) {
                                bfd_sess_info[i].skip_bfd_query= 0;
                                bfd_sess_info[i].last_rcvd_pkt_ts = onvm_util_get_current_cpu_cycles(); // make sure we start elapsed time from this interval (should be not really necessary)
                                //continue;
                        }
                }
                if(0 == bfd_sess_info[i].skip_bfd_query)
#endif
                {
                        //if(bfd_sess_info[i].remote_state !=Up || (bfd_sess_info[i].mode == BFD_SESSION_MODE_PASSIVE)) continue;
                        elapsed_time = onvm_util_get_elapsed_cpu_cycles_in_us(bfd_sess_info[i].last_rcvd_pkt_ts);
                        if(elapsed_time > BFD_TIMEOUT_INTERVAL) {
                                //Shift from Up to Down and notify Link Down Status
                                printf("Port[%d]:[%d] BFD elapsed time:%lld exceeded Allowed Time_us:%d\n", i, bfd_sess_info[i].remote_state, (long long int)elapsed_time, BFD_TIMEOUT_INTERVAL);
                                bfd_sess_info[i].remote_state = Down;
                                bfd_sess_info[i].bfd_status_change_counter++;
                                if(notifier_cb) {
                                        notifier_cb(i,BFD_STATUS_REMOTE_DOWN);
                                }
                        }
                }
        }
        return ;
}
/********************************Interfaces***********************************/
int
onvm_bfd_process_incoming_packets(__attribute__((unused)) struct thread_info *rx, struct rte_mbuf *pkts[], uint16_t rx_count) {
        uint16_t i=0;
        //printf("\n parsing Incoming BFD packets[%d]!!!\n", rx_count);
        for(;i<rx_count;i++) {
                parse_bfd_packet(pkts[i]);
        }
        bfd_stat.rx_count+=rx_count;
        return 0;
}
int
onvm_bfd_init(onvm_bfd_init_config_t *bfd_config) {
        if(unlikely(NULL == bfd_config)) return 0;
        printf("ONVM_BFD: INIT with identifier=%d(%x)\n", bfd_config->bfd_identifier, bfd_config->bfd_identifier);

        if(NULL == pktmbuf_pool) {
                pktmbuf_pool = rte_mempool_lookup(PKTMBUF_POOL_NAME);
                if(NULL == pktmbuf_pool) {
                        return -1;
                }
        }

        notifier_cb = bfd_config->cb_func;
        set_bfd_packet_template(bfd_config->bfd_identifier);
        init_bfd_session_status(bfd_config);

        //@Note: The Timer runs in the caller thread context (Main or Wakethread): Must ensure the freq is > 1/bfd interval
        initialize_bfd_timers();

        return 0;
}

int
onvm_bfd_deinit(void) {
        return 0;
}


int onvm_print_bfd_status(unsigned difftime, __attribute__((unused)) FILE *fp) {
        fprintf(fp, "BFD\n");
        fprintf(fp,"-----\n");
        uint8_t i = 0;
        static bfd_pkt_stat_t prev_stat;
        if(difftime == 0) difftime = 1;
        fprintf(fp, "rx_us:%"PRIu64" (%"PRIu64" pps) tx_us:%"PRIu64" (%"PRIu64" pps) \n",
                        bfd_stat.rx_count, (bfd_stat.rx_count - prev_stat.rx_count)/difftime,
                        bfd_stat.tx_count, (bfd_stat.tx_count - prev_stat.tx_count)/difftime);
        prev_stat = bfd_stat;
        for(i=0; i< ports->num_ports; i++) {
#ifdef PIGGYBACK_BFD_ON_ACTIVE_PORT_TRAFFIC
                fprintf(fp, "Port:%d Local status:%d, Remote Status:%d  SkipStatus:%d, last_rx_set:%d, pport_rx_pkts:%"PRIu64", last_rx_packets:%"PRIu64", Change count:%"PRIu64" last_tx(us):%"PRIu64" last_rx(us):%"PRIu64"\n",
                                i, bfd_sess_info[i].local_state,  bfd_sess_info[i].remote_state, bfd_sess_info[i].skip_bfd_query, bfd_sess_info[i].last_rx_set, ports->rx_stats.rx[i], bfd_sess_info[i].last_rx_pkts, bfd_sess_info[i].bfd_status_change_counter,
                                onvm_util_get_elapsed_cpu_cycles_in_us(bfd_sess_info[i].last_sent_pkt_ts),
                                onvm_util_get_elapsed_cpu_cycles_in_us(bfd_sess_info[i].last_rcvd_pkt_ts));
#else
                fprintf(fp, "Port:%d Local status:%d, Remote Status:%d  Change count:%"PRIu64" last_tx(us):%"PRIu64" last_rx(us):%"PRIu64"\n",
                                i, bfd_sess_info[i].local_state,  bfd_sess_info[i].remote_state, bfd_sess_info[i].bfd_status_change_counter,
                                onvm_util_get_elapsed_cpu_cycles_in_us(bfd_sess_info[i].last_sent_pkt_ts),
                                onvm_util_get_elapsed_cpu_cycles_in_us(bfd_sess_info[i].last_rcvd_pkt_ts));
#endif
        }
        return 0;
}

struct rte_mbuf* create_bfd_packet_spcl(BfdPacketHeader *pkt_hdr) {
        //printf("\n Crafting BFD packet for buffer [%p]\n", pkt);
        if(NULL == pktmbuf_pool) {
                return NULL;
        }
        struct rte_mbuf* pkt = rte_pktmbuf_alloc(pktmbuf_pool);
        if(NULL == pkt) {
                return NULL;
        }

        /* craft eth header */
        struct ether_hdr *ehdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
        /* set ether_hdr fields here e.g. */
        memset(ehdr,0, sizeof(struct ether_hdr));
        //memset(&ehdr->s_addr,0, sizeof(ehdr->s_addr));
        //memset(&ehdr->d_addr,0, sizeof(ehdr->d_addr));
        //ehdr->ether_type = rte_bswap16(ETHER_TYPE_IPv4);
        ehdr->ether_type = rte_bswap16(ETHER_TYPE_BFD);     //change to specific type for ease of packet handling.

        /* craft ipv4 header */
        struct ipv4_hdr *iphdr = (struct ipv4_hdr *)(&ehdr[1]);
        memset(iphdr,0, sizeof(struct ipv4_hdr));

        /* set ipv4 header fields here */
        struct udp_hdr *uhdr = (struct udp_hdr *)(&iphdr[1]);
        /* set udp header fields here, e.g. */
        uhdr->src_port = rte_bswap16(3784);
        uhdr->dst_port = rte_bswap16(3784);
        uhdr->dgram_len = sizeof(BfdPacket);

        BfdPacket *bfdp = (BfdPacket *)(&uhdr[1]);
        bfdp->header.flags = 0;

        rte_memcpy(bfdp, &bfd_template, sizeof(BfdPacketHeader));
        bfdp->header.myDisc = pkt_hdr->myDisc;
        bfdp->header.yourDisc = pkt_hdr->yourDisc;
        bfdp->header.flags = 2;

        //set packet properties
        size_t pkt_size = sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr) + sizeof(BfdPacket);
        pkt->data_len = pkt_size;
        pkt->pkt_len = pkt_size;

        return pkt;
}
void send_bfd_echo_packets_spcl(BfdPacketHeader *pkt_hdr) {
        struct rte_mbuf *pkt = NULL;
        pkt = create_bfd_packet_spcl(pkt_hdr);
        if(pkt) {
                bfd_send_packet_out(0, BFD_TX_PORT_QUEUE_ID, pkt);
        }

        return ;
}
