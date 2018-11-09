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
#include <gmp.h>

#include "onvm_threading.h"

static inline int GetNumCPUs(void);
pid_t Gettid(void);
//static inline int whichCoreID(int thread_no);
/*----------------------------------------------------------------------------*/
static inline int 
GetNumCPUs(void) 
{
        return sysconf(_SC_NPROCESSORS_ONLN);
}
/*----------------------------------------------------------------------------*/
pid_t 
Gettid(void)
{
        return syscall(__NR_gettid);
}
/*----------------------------------------------------------------------------*/
int
onvm_get_core(int thread_no, mpz_t _cpumask)
{
        int i, cpu_id;

        if (mpz_get_ui(_cpumask) == 0)
                return thread_no;
        else {
                int limit =  mpz_popcount(_cpumask);
                
                for (cpu_id = 0, i = 0; i < limit*4; cpu_id++)
                        if (mpz_tstbit(_cpumask, cpu_id)) {
                                /*if (thread_no == i)
                                        return cpu_id;
                                i++;*/
                                mpz_clrbit (_cpumask, cpu_id);
                                return cpu_id;//testing
                        }
        }
        return thread_no;
}
/*----------------------------------------------------------------------------*/
int 
onvm_core_affinitize(int cpu)
{
        cpu_set_t cpus;
        size_t n;
        //mpz_init(_cpumask);

        n = GetNumCPUs();

        //cpu = whichCoreID(cpu);
        
        if (cpu < 0 || cpu >= (int) n) {
                //errno = -EINVAL;
                return -1;
        }

        CPU_ZERO(&cpus);
        CPU_SET((unsigned)cpu, &cpus);

        return rte_thread_set_affinity(&cpus);
}
