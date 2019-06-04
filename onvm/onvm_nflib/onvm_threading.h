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
 * onvm_threading.h - threading helper functions
 ********************************************************************/

#ifndef _ONVM_THREADING_H_
#define _ONVM_THREADING_H_

#include "onvm_common.h"

/**
 * Gets the total number of cores.
 */
int
onvm_threading_get_num_cores(void);

/**
 * Get the core id for a new NF to run on.
 * If no flags are passed finds the core with the least number of NFs running on it,
 * will reserve the core unless the share core flag is set
 * For manual core assignment: the user picks the core
 * For shared core will allow other NFs to share assigned core
 *
 * @param core_value
 *    A pointer to set the core for NF to run on
 *    if manual core mode is used, a preferred core id is accessed from this pointer
 * @param flags
 *    Flags is a bitmask for specific core assignment options
 *    Bit MANUAL_CORE_ASSIGNMENT_BIT: for manually choosing a core by the user
 *    Bit SHARE_CORE_BIT: allow other NFs(also with SHARE_CORE_BIT enabled) to start on assigned core
 * @param cores
 *    A pointer to the core_status map containing core information
 *
 * @return
 *    0 on success
 *    NF_NO_CORES or NF_NO_DEDICATED_CORES or NF_CORE_OUT_OF_RANGE or NF_CORE_BUSY on error
 */
int
onvm_threading_get_core(uint16_t *core_value, uint8_t flags, struct core_status *cores);

/**
 * Uses the dpdk function to reaffinitize the calling pthread to another core.
 *
 * @param info
 *    An integer core value to bind the pthread to
 * @return
 *    0 on success, or a negative value on failure
 */
int
onvm_threading_core_affinitize(int core);

#endif  // _ONVM_THREADING_H_
