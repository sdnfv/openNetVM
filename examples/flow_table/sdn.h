/*********************************************************************
 *                     openNetVM
 *       https://github.com/sdnfv/openNetVM
 *
 *  Copyright 2015 George Washington University
 *            2015 University of California Riverside
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 * sdn.h - contact the controller, send request and parse response
 ********************************************************************/


#ifndef SDN_H
#define SDN_H

#include <poll.h>
#include <rte_mbuf.h>
#include "msgbuf.h"
#include "openflow.h"
#include "onvm_flow_table.h"

#define NUM_BUFFER_IDS 100000
#define ETH_ADDR_LEN 6 

enum handshake_status {
	START = 0,
	READY_TO_SEND = 99,
};


struct datapath
{
	int id;							//switch dpid
	int sock;						//secure channel, connected to controller
	struct msgbuf *inbuf, *outbuf;	//input, output buffer	
	int count;						//number of responses received???
	int xid;						//transaction id
	int switch_status;			//current switch status
	int debug;						//print debug info	
};	 

void datapath_init(struct datapath *dp, int sock, int bufsize, int debug);
void datapath_set_pollfd(struct datapath *dp, struct pollfd *pfd);
void datapath_handle_io(struct datapath *dp, const struct pollfd *pfd);
void datapath_handle_read(struct datapath *dp);
void datapath_handle_write(struct datapath *dp);
void datapath_change_status(struct datapath *dp, int new_status);
int debug_msg(struct datapath *dp, const char *msg, ...);
int make_features_reply(int switch_id, int xid, char *buf, unsigned int buflen);
int parse_set_config(struct ofp_header *msg);
int make_packet_in(int xid, uint32_t buffer_id, char *buf, struct rte_mbuf *pkt);
int make_config_reply( int xid, char *buf, int buflen);
int make_vendor_reply(int xid, char *buf,  unsigned int buflen);
int make_stats_desc_reply(struct ofp_stats_request *req, char *buf);
struct onvm_flow_key* flow_key_extract(struct ofp_match *match);
struct onvm_service_chain* flow_action_extract(struct ofp_action_header *oah, size_t actions_len);
void get_header(struct rte_mbuf  *pkt, struct ofp_packet_in *pi);
int setup_securechannel(void *);
void* run_securechannel(void *dp);
#endif
