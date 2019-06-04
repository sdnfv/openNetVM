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
 * onvm_threading.c - threading helper functions
 ********************************************************************/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "onvm_threading.h"

/*----------------------------------------------------------------------------*/
int
onvm_threading_get_num_cores(void) {
        return sysconf(_SC_NPROCESSORS_ONLN);
}

int
onvm_threading_get_core(uint16_t *core_value, uint8_t flags, struct core_status *cores) {
        int i;
        int max_cores;
        int best_core = 0;
        int pref_core_id = *core_value;
        uint16_t min_nf_count = (uint16_t)-1;

        max_cores = onvm_threading_get_num_cores();

        /* Check status of preffered core */
        if (ONVM_CHECK_BIT(flags, MANUAL_CORE_ASSIGNMENT_BIT)) {
                /* If manual core assignment and core is out of bounds */
                if (pref_core_id < 0 || pref_core_id > max_cores || !cores[pref_core_id].enabled)
                        return NF_CORE_OUT_OF_RANGE;
                /* If used as a dedicated core already */
                if (cores[pref_core_id].is_dedicated_core != 0)
                        return NF_CORE_BUSY;

                /* If dedicated core requested ensure no NFs are running on that core */
                if (!ONVM_CHECK_BIT(flags, SHARE_CORE_BIT)) {
                        if (cores[pref_core_id].nf_count == 0)
                                cores[pref_core_id].is_dedicated_core = 1;
                        else
                                return NF_NO_DEDICATED_CORES;
                }

                cores[pref_core_id].nf_count++;
                return 0;
        }

        /* Find the most optimal core, least NFs running */
        for (i = 0; i < max_cores; ++i) {
                if (cores[i].enabled && cores[i].is_dedicated_core == 0) {
                        if (cores[i].nf_count < min_nf_count) {
                                min_nf_count = cores[i].nf_count;
                                best_core = i;
                        }
                }
        }

        /* No cores available, can't launch */
        if (min_nf_count == (uint16_t)-1) {
                return NF_NO_CORES;
        }

        /* If NF wants a dedicated core and its available, reserve it */
        if (!ONVM_CHECK_BIT(flags, SHARE_CORE_BIT)) {
                if (min_nf_count == 0) {
                        cores[best_core].is_dedicated_core = 1;
                } else {
                        /* Dedicated core option not possible */
                        *core_value = best_core;
                        return NF_NO_DEDICATED_CORES;
                }
        }

        *core_value = best_core;
        cores[best_core].nf_count++;

        return 0;
}

int
onvm_threading_core_affinitize(int cpu) {
        cpu_set_t cpus;
        size_t n;

        n = onvm_threading_get_num_cores();
        if (cpu < 0 || cpu >= (int)n) {
                return -1;
        }

        CPU_ZERO(&cpus);
        CPU_SET((unsigned)cpu, &cpus);

        return rte_thread_set_affinity(&cpus);
}
