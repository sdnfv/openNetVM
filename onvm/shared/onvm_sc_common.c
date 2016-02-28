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
 * onvm_sc_common.c - service functions for manager and NFs
 ********************************************************************/

#include <inttypes.h>
#include "common.h"
#include "onvm_sc_common.h"

int
onvm_sc_append_entry(struct onvm_service_chain *chain, uint8_t action, uint16_t destination) {
	int chain_length = chain->chain_length;
	
	if (chain_length >= ONVM_MAX_CHAIN_LENGTH) {
		return 1;
	}
	
	chain->sc[chain_length].action = action;
	chain->sc[chain_length].destination = destination;
	(chain->chain_length)++;	

	return 0;
}

int
onvm_sc_set_entry(struct onvm_service_chain *chain, int entry, uint8_t action, uint16_t destination) {
	if (entry >= chain->chain_length) {
		return 1;
	}

	chain->sc[entry].action = action;
	chain->sc[entry].destination = destination;

	return 0;
} 

void 
onvm_sc_print(struct onvm_service_chain *chain) {
	int i; 
	for (i = 0; i < chain->chain_length; i++) {
		printf("cur_index:%d, action:%"PRIu8", destination:%"PRIu16"\n", 
			i, chain->sc[i].action, chain->sc[i].destination);
	}
	printf("\n");
}
