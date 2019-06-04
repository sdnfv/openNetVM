/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2019 George Washington University
 *            2015-2019 University of California Riverside
 *            2010-2019 Intel Corporation. All rights reserved.
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
 ********************************************************************/

/******************************************************************************

                                 onvm_nf.h

     This file contains the prototypes for all functions related to packet
     processing.

******************************************************************************/

#ifndef _ONVM_NF_H_
#define _ONVM_NF_H_

#include "onvm_threading.h"

/********************************Interfaces***********************************/

/*
 * Interface giving the smallest unsigned integer unused for a NF instance.
 *
 * Output : the unsigned integer
 *
 */
uint16_t
onvm_nf_next_instance_id(void);

/*
 * Interface looking through all registered NFs if one needs to start or stop.
 *
 */
void
onvm_nf_check_status(void);

/*
 * Interface to send a message to a certain NF.
 *
 * Input  : The destination NF instance ID, a constant denoting the message type
 *          (see onvm_nflib/onvm_msg_common.h), and a pointer to a data argument.
 *          The data argument should be allocated in the hugepage region (so it can
 *          be shared), i.e. using rte_malloc
 * Output : 0 if the message was successfully sent, -1 otherwise
 */
int
onvm_nf_send_msg(uint16_t dest, uint8_t msg_type, void *msg_data);

#endif  // _ONVM_NF_H_
