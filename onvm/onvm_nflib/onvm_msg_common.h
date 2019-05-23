/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2017 George Washington University
 *            2015-2017 University of California Riverside
 *            2010-2014 Intel Corporation
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
 * onvm_msg_common.h - Shared structures relating to message passing
                       between the manager and NFs
 ********************************************************************/

#ifndef _MSG_COMMON_H_
#define _MSG_COMMON_H_

#include <stdint.h>

#define MSG_FROM_MGR (0x0)
#define MSG_NOOP            (0|MSG_FROM_MGR)
#define MSG_STOP            (1|MSG_FROM_MGR)
#define MSG_PAUSE           (2|MSG_FROM_MGR)
#define MSG_RESUME          (4|MSG_FROM_MGR)
#define MSG_RUN             (MSG_RESUME)
#define MAX_MSG_FROM_MGR    (0x0F)

#define MSG_FROM_NF (0xF0)
#define MSG_NF_STARTING     (1|MSG_FROM_NF)
#define MSG_NF_STOPPING     (2|MSG_FROM_NF)
#define MSG_NF_READY        (3|MSG_FROM_NF)
#define MSG_NF_UNBLOCK_SELF (4|MSG_FROM_NF)
#define MSG_NF_REGISTER_ECB (5|MSG_FROM_NF)
#define MSG_NF_TRIGGER_ECB  (6|MSG_FROM_NF)
#define MSG_NF_SYNC_RESP    (7|MSG_FROM_NF)

#define MSG_MODE_ASYNCHRONOUS   (0)
#define MSG_MODE_SYNCHRONOUS    (1)

struct onvm_nf_msg {
        uint8_t msg_type; /* Constant saying what type of message is */
//#ifdef ENABLE_SYNC_MGR_TO_NF_MSG  //Note: This means duplicate flag here also; keep it on, but use only when necessary.
        uint8_t is_sync;  /* Indicates whether the message requires sync notification to MGR or not (default=FALSE) */
//#endif
        void *msg_data; /* These should be rte_malloc'd so they're stored in hugepages */
};

/* Enable NF--> NF_MGR notification using onvm_nf_msg rather than legacy onvm_nf_info structure: ( Disable the macro to switch to Legacy) */
#define ENABLE_MSG_CONSTRUCT_NF_INFO_NOTIFICATION
#endif
