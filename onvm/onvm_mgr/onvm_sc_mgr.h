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
 * onvm_sc_mgr.h - service chain functions for manager 
 ********************************************************************/


#ifndef _SC_MGR_H_
#define _SC_MGR_H_

#include <rte_mbuf.h>
#include "shared/common.h"

static inline uint8_t
onvm_next_action(struct onvm_service_chain* chain, uint16_t cur_nf) {
	if (cur_nf >= chain->chain_length) {
		return ONVM_NF_ACTION_DROP;
	}
	return chain->sc[cur_nf+1].action;
}

static inline uint8_t
onvm_sc_next_action(struct onvm_service_chain* chain, struct rte_mbuf* pkt) {
	return onvm_next_action(chain, onvm_get_pkt_chain_index(pkt));
}

static inline uint16_t 
onvm_next_destination(struct onvm_service_chain* chain, uint16_t cur_nf) {
	if (cur_nf + 1 > chain->chain_length) {
		return 0;
	}
	return chain->sc[cur_nf+1].destination;
}

static inline uint16_t
onvm_sc_next_destination(struct onvm_service_chain* chain, struct rte_mbuf* pkt) {
	return onvm_next_destination(chain, onvm_get_pkt_chain_index(pkt));
}

/*get service chain*/
struct onvm_service_chain* onvm_sc_get(void);
/*create service chain*/
struct onvm_service_chain* onvm_sc_create(void);
#endif  // _SC_MGR_H_
