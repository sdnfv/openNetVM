/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2017 George Washington University
 *            2015-2017 University of California Riverside
 *            2010-2014 Intel Corporation. All rights reserved.
 *            2016-2017 Hewlett Packard Enterprise Development LP
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
                                   main.c

     File containing the main function of the manager and all its worker
     threads.

******************************************************************************/


#include <signal.h>

#include "onvm_mgr.h"
#include "onvm_stats.h"
#include "onvm_pkt.h"
#include "onvm_nf.h"

struct wakeup_info *wakeup_infos;

/****************************Internal Declarations****************************/

#define MAX_SHUTDOWN_ITERS 10

// True as long as the main thread loop should keep running
static uint8_t main_keep_running = 1;

// We'll want to shut down the TX/RX threads second so that we don't
// race the stats display to be able to print, so keep this varable separate
static uint8_t worker_keep_running = 1;

static void handle_signal(int sig);

/*******************************Worker threads********************************/

/*
 * Stats thread periodically prints per-port and per-NF stats.
 */
static void
master_thread_main(void) {
        uint16_t i;
        int shutdown_iter_count;
        const unsigned sleeptime = global_stats_sleep_time;
        const unsigned verbosity_level = global_verbosity_level;

        RTE_LOG(INFO, APP, "Core %d: Running master thread\n", rte_lcore_id());

        if (stats_destination == ONVM_STATS_WEB) {
                RTE_LOG(INFO, APP, "ONVM stats can be viewed through the web console\n");
        }

        RTE_LOG(INFO, APP, "Stats verbosity level = %d\n", verbosity_level);

        /* Initial pause so above printf is seen */
        sleep(5);

        onvm_stats_init(verbosity_level);
        /* Loop forever: sleep always returns 0 or <= param */
        while ( main_keep_running && sleep(sleeptime) <= sleeptime) {
                onvm_nf_check_status();
                if (stats_destination != ONVM_STATS_NONE)
                        onvm_stats_display_all(sleeptime, verbosity_level);
        }

        /* Close out file references and things */
        onvm_stats_cleanup();

#ifdef RTE_LIBRTE_PDUMP
        rte_pdump_uninit();
#endif

        RTE_LOG(INFO, APP, "Core %d: Initiating shutdown sequence\n", rte_lcore_id());

        /* Stop all RX and TX threads */
        worker_keep_running = 0;

        /* Tell all NFs to stop */
        for (i = 0; i < MAX_NFS; i++) {
                if (nfs[i].info == NULL) {
                        continue;
                }
                RTE_LOG(INFO, APP, "Core %d: Notifying NF %"PRIu16" to shut down\n", rte_lcore_id(), i);
                onvm_nf_send_msg(i, MSG_STOP, NULL);
        }

        /* Wait to process all exits */
        for (shutdown_iter_count = 0;
             shutdown_iter_count < MAX_SHUTDOWN_ITERS && num_nfs > 0;
             shutdown_iter_count++) {
                onvm_nf_check_status();
                RTE_LOG(INFO, APP, "Core %d: Waiting for %"PRIu16" NFs to exit\n", rte_lcore_id(), num_nfs);
                sleep(sleeptime);
        }

        if (num_nfs > 0) {
                RTE_LOG(INFO, APP, "Core %d: Up to %"PRIu16" NFs may still be running and must be killed manually\n", rte_lcore_id(), num_nfs);
        }

        RTE_LOG(INFO, APP, "Core %d: Master thread done\n", rte_lcore_id());
}


/*
 * Function to receive packets from the NIC
 * and distribute them to the default service
 */
static int
rx_thread_main(void *arg) {
        uint16_t i, rx_count;
        struct rte_mbuf *pkts[PACKET_READ_SIZE];
        struct queue_mgr *rx_mgr = (struct queue_mgr*)arg;

        RTE_LOG(INFO,
                APP,
                "Core %d: Running RX thread for RX queue %d\n",
                rte_lcore_id(),
                rx_mgr->id);

        for (; worker_keep_running;) {
                /* Read ports */
                for (i = 0; i < ports->num_ports; i++) {
                        rx_count = rte_eth_rx_burst(ports->id[i], rx_mgr->id, \
                                        pkts, PACKET_READ_SIZE);
                        ports->rx_stats.rx[ports->id[i]] += rx_count;

                        /* Now process the NIC packets read */
                        if (likely(rx_count > 0)) {
                                // If there is no running NF, we drop all the packets of the batch.
                                if (!num_nfs) {
                                        onvm_pkt_drop_batch(pkts, rx_count);
                                } else {
                                        onvm_pkt_process_rx_batch(rx_mgr, pkts, rx_count);
                                }
                        }
                }
        }

        RTE_LOG(INFO, APP, "Core %d: RX thread done\n", rte_lcore_id());

        return 0;
}


static int
tx_thread_main(void *arg) {
        struct onvm_nf *nf;
        unsigned i, tx_count;
        struct rte_mbuf *pkts[PACKET_READ_SIZE];
        struct queue_mgr *tx_mgr = (struct queue_mgr *)arg;

        if (tx_mgr->tx_thread_info->first_nf == tx_mgr->tx_thread_info->last_nf - 1) {
                RTE_LOG(INFO,
                        APP,
                        "Core %d: Running TX thread for NF %d\n",
                        rte_lcore_id(),
                        tx_mgr->tx_thread_info->first_nf);
        } else if (tx_mgr->tx_thread_info->first_nf < tx_mgr->tx_thread_info->last_nf) {
                RTE_LOG(INFO,
                        APP,
                        "Core %d: Running TX thread for NFs %d to %d\n",
                        rte_lcore_id(),
                        tx_mgr->tx_thread_info->first_nf,
                        tx_mgr->tx_thread_info->last_nf-1);
        }

        for (; worker_keep_running;) {
                /* Read packets from the NF's tx queue and process them as needed */
                for (i = tx_mgr->tx_thread_info->first_nf; i < tx_mgr->tx_thread_info->last_nf; i++) {
                        nf = &nfs[i];
                        if (!onvm_nf_is_valid(nf))
                                continue;

			/* Dequeue all packets in ring up to max possible. */
			tx_count = rte_ring_dequeue_burst(nf->tx_q, (void **) pkts, PACKET_READ_SIZE, NULL);

                        /* Now process the Client packets read */
                        if (likely(tx_count > 0)) {
                                onvm_pkt_process_tx_batch(tx_mgr, pkts, tx_count, nf);
                        }
                }

                /* Send a burst to every port */
                onvm_pkt_flush_all_ports(tx_mgr);

                /* Send a burst to every NF */
                onvm_pkt_flush_all_nfs(tx_mgr, NULL);
        }

        RTE_LOG(INFO, APP, "Core %d: TX thread done\n", rte_lcore_id());

        return 0;
}

static void
handle_signal(int sig) {
        if (sig == SIGINT || sig == SIGTERM) {
                main_keep_running = 0;
        }
}

/*******************************Wake/sleep helpers****************************/
//TODO: Move to apporpriate header or a different file for onvm_nf_wakeup_mgr/hdlr.c

#define MAX_THRESHOLD 1
unsigned int nfs_wakethr[MAX_NFS] = {[0 ... MAX_NFS-1] = MAX_THRESHOLD};

static inline int
whether_wakeup_client(int instance_id)
{
        uint16_t cur_entries;
        if (nfs[instance_id].rx_q == NULL || nfs[instance_id].sem_name == NULL) {
                return 0;
        }
        cur_entries = rte_ring_count(nfs[instance_id].rx_q);
        if (cur_entries >= nfs_wakethr[instance_id]){
                return 1;
        }
        return 0;
}

static inline void
wakeup_client(int instance_id, struct wakeup_info *wakeup_info)
{
        if (whether_wakeup_client(instance_id) == 1) {
                if (rte_atomic16_read(nfs[instance_id].shm_server) ==1) {
                        wakeup_info->num_wakeups += 1;
                        rte_atomic16_set(nfs[instance_id].shm_server, 0);
                        sem_post(nfs[instance_id].mutex);
                }
        }
}

static int
wakeup_nfs(void *arg)
{
        struct wakeup_info *wakeup_info = (struct wakeup_info *)arg;
        unsigned i;

        while (true) {
                for (i = wakeup_info->first_client; i < wakeup_info->last_client; i++) {
                        wakeup_client(i, wakeup_info);
                }
        }

        return 0;
}

static void signal_handler(int sig, siginfo_t *info, void *secret) {
        int i;
        (void)info;
        (void)secret;

        //2 means terminal interrupt, 3 means terminal quit, 9 means kill and 15 means termination
        if (sig <= 15) {
                // handle for all clients or check for onvm_nf_is_valid(). Not needed!
                for (i = 1; i < MAX_NFS; i++) {
                        sem_close(nfs[i].mutex);
                        sem_unlink(nfs[i].sem_name);
                }
        }

        exit(1);
}
static void
register_signal_handler(void) {
        unsigned i;
        struct sigaction act;
        memset(&act, 0, sizeof(act));
        sigemptyset(&act.sa_mask);
        act.sa_flags = SA_SIGINFO;
        act.sa_handler = (void *)signal_handler;

        for (i = 1; i < 31; i++) {
                sigaction(i, &act, 0);
        }
}
/*******************************Main function*********************************/

int
main(int argc, char *argv[]) {
        unsigned cur_lcore, rx_lcores, tx_lcores;
        unsigned nfs_per_tx;
        unsigned i;

        /* initialise the system */
        if (ONVM_INTERRUPT_SEM) register_signal_handler();

        /* Reserve ID 0 for internal manager things */
        next_instance_id = 1;
        if (init(argc, argv) < 0 )
                return -1;
        RTE_LOG(INFO, APP, "Finished Process Init.\n");

        /* clear statistics */
        onvm_stats_clear_all_nfs();

         /* Reserve n cores for: ONVM_NUM_MGR_AUX_THREADS for auxiliary(f.e. stats), ONVM_NUM_RX_THREADS for Rx, and all remaining for Tx */
        cur_lcore = rte_lcore_id();
        rx_lcores = ONVM_NUM_RX_THREADS;
        tx_lcores = rte_lcore_count() - rx_lcores - ONVM_NUM_MGR_AUX_THREADS;
        if (ONVM_INTERRUPT_SEM) tx_lcores -= ONVM_NUM_WAKEUP_THREADS;

        /* Offset cur_lcore to start assigning TX cores */
        cur_lcore += (rx_lcores-1);

        RTE_LOG(INFO, APP, "%d cores available in total\n", rte_lcore_count());
        RTE_LOG(INFO, APP, "%d cores available for handling manager RX queues\n", rx_lcores);
        RTE_LOG(INFO, APP, "%d cores available for handling TX queues\n", tx_lcores);
        if (ONVM_INTERRUPT_SEM)
                RTE_LOG(INFO, APP, "%d cores available for handling wakeup\n", ONVM_NUM_WAKEUP_THREADS);
        RTE_LOG(INFO, APP, "%d cores available for handling stats\n", 1);

        /* Evenly assign NFs to TX threads */

        /*
         * If num NFs is zero, then we are running in dynamic NF mode.
         * We do not have a way to tell the total number of NFs running so
         * we have to calculate nfs_per_tx using MAX_NFS then.
         * We want to distribute the number of running NFs across available
         * TX threads
         */
        nfs_per_tx = ceil((float)MAX_NFS/tx_lcores);

        // We start the system with 0 NFs active
        num_nfs = 0;

        /* Listen for ^C and docker stop so we can exit gracefully */
        signal(SIGINT, handle_signal);
        signal(SIGTERM, handle_signal);

        for (i = 0; i < tx_lcores; i++) {
                struct queue_mgr *tx_mgr = calloc(1, sizeof(struct queue_mgr));
                tx_mgr->mgr_type_t = MGR;
                tx_mgr->id = i;
                tx_mgr->tx_thread_info = calloc(1, sizeof(struct tx_thread_info));
                tx_mgr->tx_thread_info->port_tx_bufs = calloc(RTE_MAX_ETHPORTS, sizeof(struct packet_buf));
                tx_mgr->nf_rx_bufs = calloc(MAX_NFS, sizeof(struct packet_buf));
                tx_mgr->tx_thread_info->first_nf = RTE_MIN(i * nfs_per_tx + 1, (unsigned)MAX_NFS);
                tx_mgr->tx_thread_info->last_nf = RTE_MIN((i+1) * nfs_per_tx + 1, (unsigned)MAX_NFS);
                cur_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
                if (rte_eal_remote_launch(tx_thread_main, (void*)tx_mgr,  cur_lcore) == -EBUSY) {
                        RTE_LOG(ERR,
                                APP,
                                "Core %d is already busy, can't use for nf %d TX\n",
                                cur_lcore,
                                tx_mgr->tx_thread_info->first_nf);
                        return -1;
                }
        }

        /* Launch RX thread main function for each RX queue on cores */
        for (i = 0; i < rx_lcores; i++) {
                struct queue_mgr *rx_mgr = calloc(1, sizeof(struct queue_mgr));
                rx_mgr->mgr_type_t = MGR;
                rx_mgr->id = i;
                rx_mgr->tx_thread_info = NULL;
                rx_mgr->nf_rx_bufs = calloc(MAX_NFS, sizeof(struct packet_buf));
                cur_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
                if (rte_eal_remote_launch(rx_thread_main, (void *)rx_mgr, cur_lcore) == -EBUSY) {
                        RTE_LOG(ERR,
                                APP,
                                "Core %d is already busy, can't use for RX queue id %d\n",
                                cur_lcore,
                                rx_mgr->id);
                        return -1;
                }
        }

        if (ONVM_INTERRUPT_SEM) {
                int clients_per_wakethread = ceil((unsigned)MAX_NFS / ONVM_NUM_WAKEUP_THREADS);
                wakeup_infos = (struct wakeup_info *)calloc(ONVM_NUM_WAKEUP_THREADS, sizeof(struct wakeup_info));
                if (wakeup_infos == NULL) {
                        printf("can not alloc space for wakeup_info\n");
                        exit(1);
                }
                for (i = 0; i < ONVM_NUM_WAKEUP_THREADS; i++) {
                        wakeup_infos[i].first_client = RTE_MIN(i * clients_per_wakethread + 1, (unsigned)MAX_NFS);
                        wakeup_infos[i].last_client = RTE_MIN((i+1) * clients_per_wakethread + 1, (unsigned)MAX_NFS);
                        cur_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
                        rte_eal_remote_launch(wakeup_nfs, (void*)&wakeup_infos[i], cur_lcore);
                        printf("wakeup lcore_id=%d, first_client=%d, last_client=%d\n", cur_lcore, wakeup_infos[i].first_client, wakeup_infos[i].last_client);
                }
        }

        /* Master thread handles statistics and NF management */
        master_thread_main();

        return 0;
}
