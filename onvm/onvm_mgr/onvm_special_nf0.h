/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2016 George Washington University
 *            2015-2016 University of California Riverside
 *            2010-2014 Intel Corporation. All rights reserved.
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
 ********************************************************************/


/******************************************************************************

                                 onvm_special_nf0.h

     This file contains the prototypes for all functions related to packet
     processing within the NF Manager's Plugin/special NF NF[0].

******************************************************************************/


#ifndef _ONVM_SPECIAL_NF0_H_
#define _ONVM_SPECIAL_NF0_H_

/********************************Globals and varaible declarations ***********************************/

/********************************Interfaces***********************************/

/*
 * Interface checking if a given nf is "valid", meaning if it's running.
 *
 * Input  : a pointer to the nf
 * Output : a boolean answer 
 *
 */
/****************************Internal functions*******************************/

/*
 * Function starting special NF.
 *
 * Input  : None
 * Output : an error code
 *
 */
int start_special_nf0(void);

/*
 * Function to Stop the special NF.
 *
 * Input  : None
 * Output : an error code
 *
 */
int stop_special_nf0(void);

/*
 * Function to start processing packets enqueued in Rx ring of special NF.
 *
 * Input  : None
 * Output : an error code
 *
 */
int process_special_nf0_rx_packets(void);

/*
 * Function to send packets to special NF.
 *
 * Input  : a pointer thread sending the packets, packets and count of packets
 * Output : an error code
 *
 */
int onv_pkt_send_to_special_nf0(__attribute__((unused)) struct thread_info *rx, __attribute__((unused)) struct rte_mbuf *pkts[], __attribute__((unused)) uint16_t rx_count);

//Add PCAP Dumper facility for replay
#ifdef ENABLE_PCAP_CAPTURE
inline int onvm_util_init_pacp_logger(int port, int mode);
inline int onvm_util_clear_pcap_log(int port);
inline int onvm_util_log_packets(int port, struct rte_mbuf **pkts, uint64_t *ts_info, uint16_t nb_pkts);
inline int onvm_util_replay_all_packets(uint8_t port, uint64_t max_duration_us);
inline const char* onvm_util_close_and_get_pdump_file(__attribute__((unused)) int port);
#endif

#endif  // _ONVM_SPECIAL_NF0_H_
