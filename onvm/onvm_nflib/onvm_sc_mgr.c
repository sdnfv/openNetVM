/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2016 George Washington University
 *            2015-2016 University of California Riverside
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
 * onvm_sc_mgr.c - service chain functions for manager 
 ********************************************************************/

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_debug.h>
#include <rte_malloc.h>
#include "onvm_sc_mgr.h"
#include "onvm_sc_common.h"


active_sc_entries_t active_sc_list;
/******************************Helper Functions********************************/
static inline int add_chain_to_active_sc_list(struct onvm_service_chain *chain);

static inline int add_chain_to_active_sc_list(struct onvm_service_chain *chain) {
        if(active_sc_list.sc_count == SDN_FT_ENTRIES) return -1;
        active_sc_list.sc[active_sc_list.sc_count++]=chain;
        return active_sc_list.sc_count;
}

/*********************************Interfaces**********************************/
const active_sc_entries_t* onvm_sc_get_all_active_chains(void) {
        return &active_sc_list;
}
struct onvm_service_chain*
onvm_sc_get(void) {
        return NULL;
}

struct onvm_service_chain*
onvm_sc_create(void)
{
        struct onvm_service_chain *chain;

        chain = rte_calloc("ONVM_sercice_chain",
                        1, sizeof(struct onvm_service_chain), 0);
        if (chain == NULL) {
                rte_exit(EXIT_FAILURE, "Cannot allocate memory for service chain\n");
        }
        add_chain_to_active_sc_list(chain);
	return chain;
}
