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
 * sdn.c - contact the controller, send request and parse response
 ********************************************************************/

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <math.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <rte_common.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "onvm_common.h"
#include "onvm_flow_dir.h"
#include "onvm_flow_table.h"
#include "onvm_nflib.h"
#include "onvm_sc_common.h"
#include "sdn.h"
#include "sdn_pkt_list.h"
#include "setupconn.h"

extern struct onvm_nf *nf;
extern struct rte_ring *ring_to_sdn;
extern struct rte_ring *ring_from_sdn;
extern uint16_t def_destination;
struct onvm_ft *pkt_buf_ft;

static struct ofp_switch_config Switch_config = {
    .header = {OFP_VERSION, OFPT_GET_CONFIG_REPLY, sizeof(struct ofp_switch_config), 0}, .flags = 0, .miss_send_len = 0,
};

static inline uint64_t
htonll(uint64_t n) {
        return htonl(1) == 1 ? n : ((uint64_t)htonl(n) << 32) | htonl(n >> 32);
}

static inline uint64_t
ntohll(uint64_t n) {
        return htonl(1) == 1 ? n : ((uint64_t)ntohl(n) << 32) | ntohl(n >> 32);
}

void
datapath_init(struct datapath *dp, int sock, int bufsize, int debug) {
        static int ID = 1;
        struct ofp_header ofph;
        dp->sock = sock;
        dp->debug = debug;
        dp->id = ID++;
        dp->inbuf = msgbuf_new(bufsize);
        dp->outbuf = msgbuf_new(bufsize);
        dp->xid = 1;
        dp->switch_status = START;

        ofph.version = OFP_VERSION;
        ofph.type = OFPT_HELLO;
        ofph.length = htons(sizeof(ofph));
        ofph.xid = htonl(1);
        msgbuf_push(dp->outbuf, (char *)&ofph, sizeof(ofph));
        debug_msg(dp, " sent hello");
}

void
datapath_set_pollfd(struct datapath *dp, struct pollfd *pfd) {
        pfd->events = POLLIN | POLLOUT;
        pfd->fd = dp->sock;
}

void
datapath_handle_io(struct datapath *dp, const struct pollfd *pfd) {
        if (pfd->revents & POLLIN)
                datapath_handle_read(dp);
        if (pfd->revents & POLLOUT)
                datapath_handle_write(dp);
}

void
datapath_handle_read(struct datapath *dp) {
        printf("DEBUG: handle read \n");
        struct ofp_header *ofph;
        struct ofp_header echo;
        struct ofp_header barrier;
        struct ofp_stats_request *stats_req;
        char buf[BUFLEN];
        int count;

        count = msgbuf_read(dp->inbuf, dp->sock);  // read any queued data
        if (count <= 0) {
                fprintf(stderr, "controller msgbuf_read() = %d:  ", count);
                if (count < 0) {
                        perror("msgbuf_read");
                } else {
                        fprintf(stderr, " closed connection ");
                }
                fprintf(stderr, "... exiting\n");
                exit(1);
        }
        while ((count = msgbuf_count_buffered(dp->inbuf)) >= (int)sizeof(struct ofp_header)) {
                ofph = msgbuf_peek(dp->inbuf);
                if (count < ntohs(ofph->length))
                        return;  // msg not all there yet
                msgbuf_pull(dp->inbuf, NULL, ntohs(ofph->length));
                switch (ofph->type) {
                        case OFPT_HELLO:
                                debug_msg(dp, "got hello");
                                // we already sent our own HELLO; don't respond
                                datapath_change_status(dp, READY_TO_SEND);
                                break;
                        case OFPT_FEATURES_REQUEST:
                                debug_msg(dp, "got feature_req");
                                count = make_features_reply(dp->id, ofph->xid, buf, BUFLEN);
                                msgbuf_push(dp->outbuf, buf, count);
                                debug_msg(dp, "sent feature_rsp");
                                datapath_change_status(dp, READY_TO_SEND);
                                break;
                        case OFPT_SET_CONFIG:
                                debug_msg(dp, "parsing set_config");
                                parse_set_config(ofph);
                                break;
                        case OFPT_GET_CONFIG_REQUEST:
                                debug_msg(dp, "got get_config_request");
                                count = make_config_reply(ofph->xid, buf, BUFLEN);
                                msgbuf_push(dp->outbuf, buf, count);
                                debug_msg(dp, "sent get_config_reply");
                                break;
                        case OFPT_VENDOR:
                                debug_msg(dp, "got vendor");
                                count = make_vendor_reply(ofph->xid, buf, BUFLEN);
                                msgbuf_push(dp->outbuf, buf, count);
                                debug_msg(dp, "sent vendor");
                                break;
                        case OFPT_ECHO_REQUEST:
                                debug_msg(dp, "got echo, sent echo_resp");
                                echo.version = OFP_VERSION;
                                echo.length = htons(sizeof(echo));
                                echo.type = OFPT_ECHO_REPLY;
                                echo.xid = ofph->xid;
                                msgbuf_push(dp->outbuf, (char *)&echo, sizeof(echo));
                                break;
                        case OFPT_BARRIER_REQUEST:
                                debug_msg(dp, "got barrier, sent barrier_resp");
                                barrier.version = OFP_VERSION;
                                barrier.length = htons(sizeof(barrier));
                                barrier.type = OFPT_BARRIER_REPLY;
                                barrier.xid = ofph->xid;
                                msgbuf_push(dp->outbuf, (char *)&barrier, sizeof(barrier));
                                break;
                        case OFPT_STATS_REQUEST:
                                stats_req = (struct ofp_stats_request *)ofph;
                                if (ntohs(stats_req->type) == OFPST_DESC) {
                                        count = make_stats_desc_reply(stats_req, buf);
                                        msgbuf_push(dp->outbuf, buf, count);
                                        debug_msg(dp, "sent description stats_reply");
                                } else {
                                        debug_msg(dp, "Silently ignoring non-desc stats_request msg\n");
                                }
                                break;
                        case OFPT_FLOW_MOD:
                                debug_msg(dp, "got flow_mod");
                                struct ofp_flow_mod *fm;
                                fm = (struct ofp_flow_mod *)ofph;
                                int ret;
                                struct onvm_ft_ipv4_5tuple *fk;
                                struct onvm_service_chain *sc;
                                struct onvm_flow_entry *flow_entry = NULL;
                                uint32_t buffer_id = ntohl(fm->buffer_id);
                                if (buffer_id == UINT32_MAX) {
                                        break;
                                }
                                struct sdn_pkt_list *sdn_list;
                                fk = flow_key_extract(&fm->match);
                                size_t actions_len = ntohs(fm->header.length) - sizeof(*fm);
                                sc = flow_action_extract(&fm->actions[0], actions_len);
                                ret = onvm_flow_dir_get_key(fk, &flow_entry);
                                if (ret == -ENOENT) {
                                        ret = onvm_flow_dir_add_key(fk, &flow_entry);
                                } else if (ret >= 0) {
                                        rte_free(flow_entry->key);
                                        rte_free(flow_entry->sc);
                                } else {
                                        rte_exit(EXIT_FAILURE, "onvm_flow_dir_get parameters are invalid");
                                }
                                memset(flow_entry, 0, sizeof(struct onvm_flow_entry));
                                flow_entry->key = fk;
                                flow_entry->sc = sc;
                                flow_entry->idle_timeout = OFP_FLOW_PERMANENT;
                                flow_entry->hard_timeout = OFP_FLOW_PERMANENT;
                                sdn_list = (struct sdn_pkt_list *)onvm_ft_get_data(pkt_buf_ft, buffer_id);
                                sdn_pkt_list_flush(nf, sdn_list);
                                break;
                        case OFPT_PORT_MOD:
                                debug_msg(dp, "got port_mod");
                                break;
                        case OFPT_PACKET_OUT:
                                debug_msg(dp, "got packet_out");
                                break;
                        case OFPT_ERROR:
                                debug_msg(dp, "got error");
                                break;
                        default:
                                fprintf(stderr, "Ignoring OpenFlow message type %d\n", ofph->type);
                }
        }
}

void
datapath_handle_write(struct datapath *dp) {
        char buf[BUFLEN];
        int count = 0;
        int buffer_id = 0;

        if (dp->switch_status == READY_TO_SEND) {
                struct rte_mbuf *pkt;
                int ret;

                ret = rte_ring_dequeue(ring_to_sdn, (void **)&pkt);
                if (ret == 0) {
                        struct sdn_pkt_list *flow;
                        ret = onvm_ft_lookup_pkt(pkt_buf_ft, pkt, (char **)&flow);
                        if (ret == -ENOENT) {
#ifdef DEBUG_PRINT
                                printf("SDN: not in pkt buffer table, creating list. RSS=%d port=%d\n", pkt->hash.rss,
                                       pkt->port);
#endif
                                ret = onvm_ft_add_pkt(pkt_buf_ft, pkt, (char **)&flow);
                                if (ret == -ENOSPC) {
#ifdef DEBUG_PRINT
                                        printf("Pkt buffer table no space\n");
#endif
                                        exit(1);
                                }
                        }
                        buffer_id = ret;
#ifdef DEBUG_PRINT
                        printf("SDN: in pkt buffer table, adding to list. RSS=%d port=%d\n", pkt->hash.rss, pkt->port);
#endif
                        sdn_pkt_list_add(flow, pkt);
                        memset(buf, 0, BUFLEN);
                        if (sdn_pkt_list_get_flag(flow) == 0) {
                                count = make_packet_in(dp->xid++, buffer_id, buf, pkt);
                                sdn_pkt_list_set_flag(flow);
                                if (count > 0) {
                                        msgbuf_push(dp->outbuf, buf, count);
                                }
                        }
                }
        }

        if (msgbuf_count_buffered(dp->outbuf) > 0) {
                msgbuf_write(dp->outbuf, dp->sock, 0);
        }
}

void
datapath_change_status(struct datapath *dp, int new_status) {
        dp->switch_status = new_status;
        debug_msg(dp, "next status %d", new_status);
}

int
parse_set_config(struct ofp_header *msg) {
        struct ofp_switch_config *sc;
        assert(msg->type == OFPT_SET_CONFIG);
        sc = (struct ofp_switch_config *)msg;
        memcpy(&Switch_config, sc, sizeof(struct ofp_switch_config));

        return 0;
}

int
make_config_reply(int xid, char *buf, int buflen) {
        int len = sizeof(struct ofp_switch_config);
        assert(buflen >= len);
        Switch_config.header.type = OFPT_GET_CONFIG_REPLY;
        Switch_config.header.xid = xid;
        memcpy(buf, &Switch_config, len);

        return len;
}

int
make_features_reply(int id, int xid, char *buf, unsigned int buflen) {
        struct ofp_switch_features *features;
        const char fake[] =  {
                0x97, 0x06, 0x00, 0xe0, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x76, 0xa9, 0xd4, 0x0d, 0x25, 0x48,
                0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xff,
                0x00, 0x01, 0x1a, 0xc1, 0x51, 0xff, 0xef, 0x8a, 0x76, 0x65, 0x74, 0x68, 0x31, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x02, 0xce, 0x2f, 0xa2, 0x87, 0xf6, 0x70, 0x76, 0x65, 0x74, 0x68, 0x33, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x03, 0xca, 0x8a, 0x1e, 0xf3, 0x77, 0xef, 0x76, 0x65, 0x74, 0x68, 0x35, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x04, 0xfa, 0xbc, 0x77, 0x8d, 0x7e, 0x0b, 0x76, 0x65, 0x74, 0x68, 0x37, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };

        assert(buflen > sizeof(fake));
        memcpy(buf, fake, sizeof(fake));
        features = (struct ofp_switch_features *)buf;
        features->header.version = OFP_VERSION;
        features->header.xid = xid;
        features->datapath_id = htonll(id);

        return sizeof(fake);
}

int
make_stats_desc_reply(struct ofp_stats_request *req, char *buf) {
        static struct ofp_desc_stats cbench_desc = {.mfr_desc = "Cbench - controller I/O benchmark",
                                                    .hw_desc = "this is actually software...",
                                                    // .sw_desc  = "version " VERSION,
                                                    .sw_desc = "version 1",
                                                    .serial_num = "none",
                                                    .dp_desc = "none"};
        struct ofp_stats_reply *reply;
        int len = sizeof(struct ofp_stats_reply) + sizeof(struct ofp_desc_stats);

        assert(BUFLEN > len);
        assert(ntohs(req->type) == OFPST_DESC);
        memcpy(buf, req, sizeof(*req));
        reply = (struct ofp_stats_reply *)buf;
        reply->header.type = OFPT_STATS_REPLY;
        reply->header.length = htons(len);
        reply->flags = 0;
        memcpy(reply->body, &cbench_desc, sizeof(cbench_desc));

        return len;
}

int
make_vendor_reply(int xid, char *buf, unsigned int buflen) {
        struct ofp_error_msg *e;

        assert(buflen > sizeof(struct ofp_error_msg));
        e = (struct ofp_error_msg *)buf;
        e->header.type = OFPT_ERROR;
        e->header.version = OFP_VERSION;
        e->header.length = htons(sizeof(struct ofp_error_msg));
        e->header.xid = xid;
        e->type = htons(OFPET_BAD_REQUEST);
        e->code = htons(OFPBRC_BAD_VENDOR);

        return sizeof(struct ofp_error_msg);
}
int
debug_msg(struct datapath *dp, const char *msg, ...) {
        va_list aq;

        if (dp->debug == 0)
                return 0;
        fprintf(stderr, "-------Switch %d: ", dp->id);
        va_start(aq, msg);
        vfprintf(stderr, msg, aq);
        if (msg[strlen(msg) - 1] != '\n')
                fprintf(stderr, "\n");
        // fflush(stderr);     // should be redundant, but often isn't :-(

        return 1;
}

void
get_header(struct rte_mbuf *pkt, struct ofp_packet_in *pi) {
        char *header;
        header = (char *)(rte_pktmbuf_mtod(pkt, unsigned char *));
        memcpy(pi->data, header, OFP_DEFAULT_MISS_SEND_LEN);
}

int
make_packet_in(int xid, uint32_t buffer_id, char *buf, struct rte_mbuf *pkt) {
        struct ofp_packet_in *pi;
        struct onvm_pkt_meta *meta;
        int len;

        pi = (struct ofp_packet_in *)buf;
        pi->header.version = OFP_VERSION;
        pi->header.type = OFPT_PACKET_IN;
        pi->header.xid = htonl(xid);
        meta = onvm_get_pkt_meta(pkt);
        pi->in_port = htons(meta->src);
        pi->buffer_id = htonl(buffer_id);
        pi->reason = OFPR_NO_MATCH;
        pi->pad = 0;
        get_header(pkt, pi);

        len = sizeof(*pi) + OFP_DEFAULT_MISS_SEND_LEN;
        pi->header.length = htons(len);
        pi->total_len = htons(len);
        return len;
}

struct onvm_ft_ipv4_5tuple *
flow_key_extract(struct ofp_match *match) {
        struct onvm_ft_ipv4_5tuple *fk;
        fk = rte_calloc("flow_key", 1, sizeof(struct onvm_ft_ipv4_5tuple), 0);
        if (fk == NULL) {
                rte_exit(EXIT_FAILURE, "Cannot allocate memory for flow key\n");
        }

        fk->src_addr = match->nw_src;
        fk->dst_addr = match->nw_dst;
        fk->proto = match->nw_proto;
        fk->src_port = match->tp_src;
        fk->dst_port = match->tp_dst;

        return fk;
}

struct onvm_service_chain *
flow_action_extract(struct ofp_action_header *oah, size_t actions_len) {
        uint8_t *p = (uint8_t *)oah;
        struct onvm_service_chain *chain;

        chain = rte_calloc("service chain", 1, sizeof(struct onvm_service_chain), 0);
        if (chain == NULL) {
                rte_exit(EXIT_FAILURE, "Cannot allocate memory for service chain\n");
        }

        if (actions_len == 0) {
                onvm_sc_append_entry(chain, ONVM_NF_ACTION_DROP, 0);
        } else {
                while (actions_len > 0) {
                        struct ofp_action_header *ah = (struct ofp_action_header *)p;
                        size_t len = htons(ah->len);
                        if (ah->type == htons(OFPAT_OUTPUT)) {  // OFPAT_OUTPUT action
                                struct ofp_action_output *oao = (struct ofp_action_output *)p;
                                onvm_sc_append_entry(chain, ONVM_NF_ACTION_OUT, ntohs(oao->port));
                        } else if (ah->type == htons(OFPAT_ENQUEUE)) {
                                struct ofp_action_enqueue *oae = (struct ofp_action_enqueue *)p;
                                onvm_sc_append_entry(chain, ONVM_NF_ACTION_TONF, (uint16_t)(ntohl(oae->queue_id)));
                        }
                        p += len;
                        actions_len -= len;
                }
        }

        return chain;
}

int
setup_securechannel(void *ptr) {
        (void)ptr;  // TODO: should take SDN controller IP/port as argument
        struct datapath *dp;
        int sock;
        const char *controller_hostname = "localhost";
        int controller_port = 6633;
        int debug = 1;
        struct sdn_pkt_list *sdn_list;
        int i;

        printf("SDN Connection running on %d\n", rte_lcore_id());

        fprintf(stderr,
                "Connecting to controller at %s:%d \n"
                "Debugging info is %s\n",
                controller_hostname, controller_port, debug == 1 ? "on" : "off");

        dp = malloc(sizeof(struct datapath));
        assert(dp);
        sock = make_tcp_connection(controller_hostname, controller_port);  // FIXIT
        if (sock < 0) {
                fprintf(stderr, "make_nonblock_tcp_connection :: returned %d", sock);
                exit(1);
        }

        if (debug)
                fprintf(stderr, "Initializing switch...\n");
        fflush(stderr);
        datapath_init(dp, sock, 65536, debug);

        if (debug)
                fprintf(stderr, " :: done.\n");
        fflush(stderr);

        if (debug)
                fprintf(stderr, "Creating pkt buffer table...\n");
        pkt_buf_ft = onvm_ft_create(1024, sizeof(struct sdn_pkt_list));
        if (pkt_buf_ft == NULL) {
                rte_exit(EXIT_FAILURE, "Unable to create pkt buffer table\n");
        }
        for (i = 0; i < 1024; i++) {
                sdn_list = (struct sdn_pkt_list *)onvm_ft_get_data(pkt_buf_ft, i);
                sdn_pkt_list_init(sdn_list);
        }

        if (debug)
                fprintf(stderr, "Running secure channel\n");
        run_securechannel(dp);
        return 0;
}

/* loop secure channel thread  and monitor io event */
void *
run_securechannel(void *dp) {
        struct pollfd *pollfds;
        dp = (struct datapath *)dp;
        int n_switches = 1;

        pollfds = malloc(n_switches * sizeof(struct pollfd));
        assert(pollfds);
        while (1) {
                datapath_set_pollfd(dp, pollfds);
                poll(pollfds, n_switches, 1000);
                datapath_handle_io(dp, pollfds);
        }
        free(pollfds);
}
