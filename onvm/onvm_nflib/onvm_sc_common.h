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
 * onvm_sc_common.h - service functions for manager and NFs
 ********************************************************************/

#ifndef _ONVM_SC_COMMON_H_
#define _ONVM_SC_COMMON_H_

#include <inttypes.h>
#include "onvm_common.h"

/********************************Global variables*****************************/

extern struct onvm_nf *nfs;
extern uint16_t **services;
extern uint16_t *nf_per_service_count;

/********************************Interfaces***********************************/

uint16_t
onvm_sc_service_to_nf_map(uint16_t service_id,
                          struct rte_mbuf *pkt); /*, uint16_t *nf_per_service_count, uint16_t **services);*/

/* append a entry to serivce chain, 0 means appending successful, 1 means failed*/
int
onvm_sc_append_entry(struct onvm_service_chain *chain, uint8_t action, uint16_t destination);

/*set entry to a new action and destination, 0 means setting successful, 1 means failed */
int
onvm_sc_set_entry(struct onvm_service_chain *chain, int entry, uint8_t action, uint16_t destination);

void
onvm_sc_print(struct onvm_service_chain *chain);

#endif // _ONVM_SC_COMMON_H_
