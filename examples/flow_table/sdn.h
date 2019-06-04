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
 * sdn.h - contact the controller, send request and parse response
 ********************************************************************/

#ifndef _SDN_H_
#define _SDN_H_

#include <poll.h>
#include <rte_mbuf.h>
#include "msgbuf.h"
#include "onvm_flow_table.h"
#include "openflow.h"

#define NUM_BUFFER_IDS 100000
#define ETH_ADDR_LEN 6

enum handshake_status {
        START = 0,
        READY_TO_SEND = 99,
};

struct datapath {
        int id;                         // switch dpid
        int sock;                       // secure channel, connected to controller
        struct msgbuf *inbuf, *outbuf;  // input, output buffer
        int count;                      // number of responses received???
        int xid;                        // transaction id
        int switch_status;              // current switch status
        int debug;                      // print debug info
};

void
datapath_init(struct datapath *dp, int sock, int bufsize, int debug);
void
datapath_set_pollfd(struct datapath *dp, struct pollfd *pfd);
void
datapath_handle_io(struct datapath *dp, const struct pollfd *pfd);
void
datapath_handle_read(struct datapath *dp);
void
datapath_handle_write(struct datapath *dp);
void
datapath_change_status(struct datapath *dp, int new_status);
int
debug_msg(struct datapath *dp, const char *msg, ...);
int
make_features_reply(int switch_id, int xid, char *buf, unsigned int buflen);
int
parse_set_config(struct ofp_header *msg);
int
make_packet_in(int xid, uint32_t buffer_id, char *buf, struct rte_mbuf *pkt);
int
make_config_reply(int xid, char *buf, int buflen);
int
make_vendor_reply(int xid, char *buf, unsigned int buflen);
int
make_stats_desc_reply(struct ofp_stats_request *req, char *buf);
struct onvm_ft_ipv4_5tuple *
flow_key_extract(struct ofp_match *match);
struct onvm_service_chain *
flow_action_extract(struct ofp_action_header *oah, size_t actions_len);
void
get_header(struct rte_mbuf *pkt, struct ofp_packet_in *pi);
int
setup_securechannel(void *);
void *
run_securechannel(void *dp);

#endif // _SDN_H_
