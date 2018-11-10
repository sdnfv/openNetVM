/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2018 George Washington University
 *            2015-2018 University of California Riverside
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

/***************************Standard C library********************************/
/******************************DPDK libraries*********************************/

/*****************************Internal headers********************************/



/**********************************Macros*************************************/

/******************************Global Variables*******************************/

/***********************Internal Functions Prototypes*************************/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <rte_per_lcore.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_lcore.h>

#include "onvm_threading.h"

/*----------------------------------------------------------------------------*/
int 
GetNumCPUs(void) 
{
        return sysconf(_SC_NPROCESSORS_ONLN);
}

int
onvm_get_core(int preferred_core, struct core_status *cores)
{
	int i;
	int best_core = 0;
	int min_nf_count = 10000;
	
	for (i=0;i<64;i++){
		if (cores[i].enabled) {
			if (preferred_core == i) {
				best_core = i;
				break;
			} else if (cores[i].nf_count == 0) {
				best_core = i;
				break;
			} else if (cores[i].nf_count < min_nf_count) {
				best_core = i;
				min_nf_count = cores[i].nf_count;
			}
			
		}
	}
	cores[best_core].nf_count++;

	return best_core;
}

int 
onvm_core_affinitize(int cpu)
{
        cpu_set_t cpus;
        size_t n;

	n = GetNumCPUs();
        if (cpu < 0 || cpu >= (int) n) {
                return -1;
        }

        CPU_ZERO(&cpus);
        CPU_SET((unsigned)cpu, &cpus);

        return rte_thread_set_affinity(&cpus);
}
