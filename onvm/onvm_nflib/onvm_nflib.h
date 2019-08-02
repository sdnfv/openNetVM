/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2019 George Washington University
 *            2015-2019 University of California Riverside
 *            2016-2019 Hewlett Packard Enterprise Development LP
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

                                onvm_nflib.h


                           Header file for the API


******************************************************************************/

#ifndef _ONVM_NFLIB_H_
#define _ONVM_NFLIB_H_
#include <rte_mbuf.h>
#include "onvm_common.h"
#include "onvm_pkt_common.h"
#include "onvm_threading.h"

/************************************API**************************************/

/**
 * Initialize the starting OpenNetVM NF context.
 *
 * @return
 * Pointer to the created NF context
 */
struct onvm_nf_local_ctx *
onvm_nflib_init_nf_local_ctx(void);

/**
 * Initialize an empty NF functional table
 *
 * @return
 * Pointer to the created function table struct
 */
struct onvm_nf_function_table *
onvm_nflib_init_nf_function_table(void);

/**
 * Initialize the default OpenNetVM signal handling.
 *
 * @param nf_local_ctx
 *   Pointer to a context struct of this NF.
 * @param signal_handler
 *   Function pointer to an optional NF specific signal handler function,
 *   that will be called after the default onvm signal handler.
 * @return
 * Error code or 0 if succesfull 
 */
int
onvm_nflib_start_signal_handler(struct onvm_nf_local_ctx *nf_local_ctx, handle_signal_func signal_hanlder);

/**
 * Initialize the OpenNetVM container Library.
 * This will setup the DPDK EAL as a secondary process, and notify the host
 * that there is a new NF.
 *
 * @argc
 *   The argc argument that was given to the main() function.
 * @argv
 *   The argv argument that was given to the main() function
 * @param tag
 *   A uniquely identifiable string for this NF.
 *   For example, can be the application name (e.g. "bridge_nf")
 * @param nf_local_ctx
 *   Pointer to a context struct of this NF.
 * @param nf_function_table
 *   Pointer to a function table struct for this NF
 * @return
 *   On success, the number of parsed arguments, which is greater or equal to
 *   zero. After the call to onvm_nf_init(), all arguments argv[x] with x < ret
 *   may be modified and should not be accessed by the application.,
 *   On error, a negative value.
 */
int
onvm_nflib_init(int argc, char *argv[], const char *nf_tag, struct onvm_nf_local_ctx *nf_local_ctx,
                struct onvm_nf_function_table *nf_function_table);

/**
 * Runs the OpenNetVM container library.
 *
 * @param nf_local_ctx
 *   Pointer to a context struct of this NF.
 * @return
 *   0 on success, or a negative value on error.
 */
int
onvm_nflib_run(struct onvm_nf_local_ctx *nf_local_ctx);

/**
 * Return a packet that was created by the NF or has previously had the
 * ONVM_NF_ACTION_BUFFER action called on it.
 *
 * @param nf
 *    Pointer to a struct containing information about this NF.
 * @param pkt
 *    a pointer to a packet that should now have a action other than buffer.
 * @return
 *    0 on success, or a negative value on error.
 */
int
onvm_nflib_return_pkt(struct onvm_nf *nf, struct rte_mbuf *pkt);

/**
 * Return a group of packets that were created by the NF or have previously had the
 * ONVM_NF_ACTION_BUFFER action called on it.
 *
 * @param pkts
 *    a pointer to a buffer of packets that should now have an action other than buffer.
 * @param count
 *    the number of packets contained within the buffer.
 * @return
 *    0 on success, or a negative value on error (-1 if bad arguments, -ENOBUFS if enqueue fails).
 */
int
onvm_nflib_return_pkt_bulk(struct onvm_nf *nf, struct rte_mbuf **pkts, uint16_t count);

/**
 * Inform the manager that the NF is ready to receive packets.
 * This only needs to be called when the NF is using advanced rings
 * Otherwise, onvm_nflib_run will call this
 *
 * @param nf
 *    Pointer to a struct containing information about this NF.
 * @return
 *    0 on success, or a negative value on failure
 */
int
onvm_nflib_nf_ready(struct onvm_nf *nf);

/*
 * Start the NF by signaling manager that its ready to recieve packets
 *
 * Input: Pointer to context struct of this NF
 */
int
onvm_nflib_start_nf(struct onvm_nf_local_ctx *nf_local_ctx, struct onvm_nf_init_cfg *nf_init_cfg);

/**
 * Process an message. Does stuff.
 *
 * @param msg
 *    a pointer to a message to be processed
 * @param nf_local_ctx
 *   Pointer to a context struct of this NF.
 * @return
 *    0 on success, or a negative value on error
 */
int
onvm_nflib_handle_msg(struct onvm_nf_msg *msg, struct onvm_nf_local_ctx *nf_local_ctx);

int
onvm_nflib_send_msg_to_nf(uint16_t dest_nf, void *msg_data);

/**
 * Stop this NF and clean up its memory
 * Sends shutdown message to manager.
 *
 * @param nf_local_ctx
 *   Pointer to a context struct of this NF.
 */
void
onvm_nflib_stop(struct onvm_nf_local_ctx *nf_local_ctx);

/**
 * Function that initialize the NF init config data structure.
 *
 * Input  : the tag to name the NF
 * Output : the data structure initialized
 *
 */
struct onvm_nf_init_cfg *
onvm_nflib_init_nf_init_cfg(const char *tag);

/*
 * Function that initialize the NF init config data structure.
 * the arguments are copied from the parent information
 *
 * Input  : pointer to the parent NF
 * Output : the data structure initialized
 *
 */
struct onvm_nf_init_cfg *
onvm_nflib_inherit_parent_init_cfg(struct onvm_nf *parent);

/**
 * Allocates an empty scaling config to be filled in by the NF.
 * Defines the instance_id to NF_NO_ID..
 *
 * @param nf
 *   An onvm_nf struct describing this NF.
 * @return
 *   Pointer to onvm_nf_scale_info structure for running onvm_nflib_scale
 */
struct onvm_nf_scale_info *
onvm_nflib_get_empty_scaling_config(struct onvm_nf *nf);

/**
 * Fill the onvm_nflib_scale_info with the infromation of the parent, inherits
 * service id, pkt functions(setup, pkt_handler, callback, advanced rings).
 *
 * @param nf
 *   An onvm_nf struct describing this NF.
 * @param data
 *   Void data pointer for the scale_info.
 * @return
 *   Pointer to onvm_nf_scale_info structure which can be used to run onvm_nflib_scale
 */
struct onvm_nf_scale_info *
onvm_nflib_inherit_parent_config(struct onvm_nf *nf, void *data);

/*
 * Scales the NF. Determines the core to scale to, and starts a new thread for the NF.
 *
 * @param scale_info
 *   A scale info struct describing the child NF to be launched
 * @return
 *   Error code or 0 if successful.
 */
int
onvm_nflib_scale(struct onvm_nf_scale_info *scale_info);

/**
 * Request LPM memory region. Returns the success or failure of this initialization.
 *
 * @param lpm_request
 *   An LPM request struct to initialize the LPM region
 * @return
 *   Request response status
 */
int
onvm_nflib_request_lpm(struct lpm_request *req);

struct onvm_service_chain *
onvm_nflib_get_default_chain(void);

/**
 * Retrieves custom onvm flags
 */
struct onvm_configuration *
onvm_nflib_get_onvm_config(void);

#endif // _ONVM_NFLIB_H_
