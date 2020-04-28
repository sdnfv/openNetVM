/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2020 National Institute of Technology Karnataka
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
 * fair_queue.c - Categorize pkts based on 5-tuple (src/dst IP, src/dst
 *      port, protocol) header values into separate queues and apply
 *      individual token buckets to each queue.
 ********************************************************************/

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_ip.h>
#include <rte_mbuf.h>

#include "cJSON.h"
#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"

#include "fair_queue_helper.h"

#define NF_TAG "fair_queue"

#define PKT_READ_SIZE ((uint16_t)32)

#define FQ_STATS_MSG                                                       \
        "\n"                                                               \
        "QUEUE ID         rx_pps  /  tx_pps             rx  /  tx      \n" \
        "               drop_pps  /  drop_pps      rx_drop  /  tx_drop \n" \
        "--------------------------------------------------------------\n"

#define FQ_STATS_CONTENT                                                    \
        "%2u             %9" PRIu64 " / %-9" PRIu64 "   %11" PRIu64 " / %-11" PRIu64 "\n"\
        "               %9" PRIu64 " / %-9" PRIu64 "   %11" PRIu64 " / %-11" PRIu64 "\n\n"

static uint32_t destination;

/* For advanced rings scaling */
rte_atomic16_t signal_exit_flag;
struct child_spawn_info {
        struct onvm_nf_init_cfg *child_cfg;
        struct onvm_nf *parent;
};

static struct fairq_t *fairq;
char *config_file = NULL;
static struct token_bucket_config *config;

struct token_bucket_config *
parse_config_file(uint8_t *num_queues, char *config_file);

void
sig_handler(int sig);

void *
start_child(void *arg);

void *
do_fq_stats_display(void *arg);

/*
 * Print a usage message
 */
static void
usage(const char *progname) {
        printf("Usage:\n");
        printf("%s [EAL args] -- [NF_LIB args] -- -d <destination> -p <print_delay> -D <tb_depth> -R <tb_rate>\n",
               progname);
        printf("%s -F <CONFIG_FILE.json> [EAL args] -- [NF_LIB args] -- [NF args]\n\n", progname);
        printf("Flags:\n");
        printf(" - `-d <dst>`: destination service ID to foward to\n");
        printf(
            " - `-f <config_file>`: Path to the JSON fair queue configuration file; Refer README for the "
            "structure.\n");
}

/*
 * Parse the application arguments.
 */
static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c, dst_flag = 0, config_flag = 0;

        while ((c = getopt(argc, argv, "d:f:")) != -1) {
                switch (c) {
                        case 'd':
                                destination = strtoul(optarg, NULL, 10);
                                dst_flag = 1;
                                break;
                        case 'f':
                                config_file = strdup(optarg);
                                config_flag = 1;
                                break;
                        case '?':
                                usage(progname);
                                if (optopt == 'd')
                                        RTE_LOG(INFO, APP, "Option -%c requires an argument.\n", optopt);
                                else if (optopt == 'f')
                                        RTE_LOG(INFO, APP, "Option -%c requires an argument.\n", optopt);
                                else if (isprint(optopt))
                                        RTE_LOG(INFO, APP, "Unknown option `-%c'.\n", optopt);
                                else
                                        RTE_LOG(INFO, APP, "Unknown option character `\\x%x'.\n", optopt);
                                return -1;
                        default:
                                usage(progname);
                                return -1;
                }
        }

        if (!dst_flag) {
                RTE_LOG(INFO, APP, "Fair Queue NF requires destination flag -d.\n");
                return -1;
        }

        if (!config_flag) {
                RTE_LOG(INFO, APP, "Default queue configurations used. Specify a config file using -f FILE_PATH.\n");
        }

        return optind;
}

void *
do_fq_stats_display(__attribute__((unused)) void *arg) {
        const char clr[] = {27, '[', '2', 'J', '\0'};
        const char topLeft[] = {27, '[', '1', ';', '1', 'H', '\0'};

        while (!rte_atomic16_read(&signal_exit_flag)) {
                printf("%s%s", clr, topLeft);
                printf(FQ_STATS_MSG);

                for (uint8_t i = 0; i < fairq->num_queues; i++) {
                        struct fq_queue *fq;
                        uint64_t rx, rx_last, tx, tx_last;
                        uint64_t rx_drop, tx_drop, rx_drop_last, tx_drop_last;

                        fq = fairq->fq[i];

                        rx = fq->rx;
                        rx_last = fq->rx_last;
                        tx = fq->tx;
                        tx_last = fq->tx_last;
                        rx_drop = fq->rx_drop;
                        rx_drop_last = fq->rx_drop_last;
                        tx_drop = fq->tx_drop;
                        tx_drop_last = fq->tx_drop_last;

                        printf(FQ_STATS_CONTENT, i + 1, rx - rx_last, tx - tx_last, rx, tx, rx_drop - rx_drop_last,
                               tx_drop - tx_drop_last, rx_drop, tx_drop);

                        fq->rx_last = rx;
                        fq->tx_last = tx;
                        fq->rx_drop_last = rx_drop;
                        fq->tx_drop_last = tx_drop;
                }
                sleep(1);
        }

        return NULL;
}

void
sig_handler(int sig) {
        if (sig != SIGINT && sig != SIGTERM)
                return;

        /* Will stop the processing for all spawned threads in advanced rings mode */
        rte_atomic16_set(&signal_exit_flag, 1);
}

static int
tx_loop(struct onvm_nf_local_ctx *nf_local_ctx) {
        void *pktsTX[PKT_READ_SIZE];
        int tx_batch_size;
        struct onvm_pkt_meta *meta;
        struct rte_ring *tx_ring;
        struct rte_ring *msg_q;
        struct onvm_nf *nf;
        struct onvm_nf_msg *msg;
        struct rte_mempool *nf_msg_pool;
        struct rte_mbuf *pkt;

        nf = nf_local_ctx->nf;

        onvm_nflib_nf_ready(nf);

        /* Get rings from nflib */
        tx_ring = nf->tx_q;
        msg_q = nf->msg_q;
        nf_msg_pool = rte_mempool_lookup(_NF_MSG_POOL_NAME);

        printf("Process %d handling packets using advanced rings\n", nf->instance_id);
        if (onvm_threading_core_affinitize(nf->thread_info.core) < 0)
                rte_exit(EXIT_FAILURE, "Failed to affinitize to core %d\n", nf->thread_info.core);

        tx_batch_size = 0;
        while (!rte_atomic16_read(&signal_exit_flag)) {
                /* Check for a stop message from the manager */
                if (unlikely(rte_ring_count(msg_q) > 0)) {
                        msg = NULL;
                        rte_ring_dequeue(msg_q, (void **)(&msg));
                        if (msg->msg_type == MSG_STOP) {
                                rte_atomic16_set(&signal_exit_flag, 1);
                        } else {
                                printf("Received message %d, ignoring", msg->msg_type);
                        }
                        rte_mempool_put(nf_msg_pool, (void *)msg);
                }

                /* Dequeue packet from the fair queue system */
                pkt = NULL;
                fairq_dequeue(fairq, &pkt);

                if (pkt == NULL) {
                        continue;
                }

                meta = onvm_get_pkt_meta((struct rte_mbuf *)pkt);
                meta->action = ONVM_NF_ACTION_TONF;
                meta->destination = destination;
                pktsTX[tx_batch_size++] = pkt;
                if (tx_batch_size == PKT_READ_SIZE) {
                        rte_ring_enqueue_bulk(tx_ring, pktsTX, tx_batch_size, NULL);
                        tx_batch_size = 0;
                }
        }
        return 0;
}

static int
rx_loop(struct onvm_nf_local_ctx *nf_local_ctx) {
        void *pkts[PKT_READ_SIZE];
        void *pktsDrop[PKT_READ_SIZE];
        struct onvm_pkt_meta *meta;
        uint16_t i, nb_pkts;
        int tx_batch_size;
        struct rte_ring *rx_ring;
        struct rte_ring *tx_ring;
        struct rte_ring *msg_q;
        struct onvm_nf *nf;
        struct onvm_nf_msg *msg;
        struct rte_mempool *nf_msg_pool;

        nf = nf_local_ctx->nf;
        onvm_nflib_nf_ready(nf);

        /* Get rings from nflib */
        rx_ring = nf->rx_q;
        tx_ring = nf->tx_q;
        msg_q = nf->msg_q;
        nf_msg_pool = rte_mempool_lookup(_NF_MSG_POOL_NAME);

        printf("Process %d handling packets using advanced rings\n", nf->instance_id);
        if (onvm_threading_core_affinitize(nf->thread_info.core) < 0)
                rte_exit(EXIT_FAILURE, "Failed to affinitize to core %d\n", nf->thread_info.core);
        printf("entering while loop\n");

        tx_batch_size = 0;
        while (!rte_atomic16_read(&signal_exit_flag)) {
                /* Check for a stop message from the manager */
                if (unlikely(rte_ring_count(msg_q) > 0)) {
                        msg = NULL;
                        rte_ring_dequeue(msg_q, (void **)(&msg));
                        if (msg->msg_type == MSG_STOP) {
                                rte_atomic16_set(&signal_exit_flag, 1);
                        } else {
                                printf("Received message %d, ignoring", msg->msg_type);
                        }
                        rte_mempool_put(nf_msg_pool, (void *)msg);
                }

                nb_pkts = rte_ring_dequeue_burst(rx_ring, pkts, PKT_READ_SIZE, NULL);

                for (i = 0; i < nb_pkts; i++) {
                        if (fairq_enqueue(fairq, pkts[i]) == -1) {
                                meta = onvm_get_pkt_meta((struct rte_mbuf *)pkts[i]);
                                meta->action = ONVM_NF_ACTION_DROP;
                                meta->destination = destination;
                                pktsDrop[tx_batch_size++] = pkts[i];
                                if (tx_batch_size == 32) {
                                        rte_ring_enqueue_bulk(tx_ring, pktsDrop, tx_batch_size, NULL);
                                        tx_batch_size = 0;
                                }
                        }
                }
        }
        return 0;
}

void *
start_child(void *arg) {
        struct onvm_nf_local_ctx *child_local_ctx;
        struct onvm_nf_init_cfg *child_init_cfg;
        struct onvm_nf *parent;
        struct child_spawn_info *spawn_info;

        spawn_info = (struct child_spawn_info *)arg;
        child_init_cfg = spawn_info->child_cfg;
        parent = spawn_info->parent;
        child_local_ctx = onvm_nflib_init_nf_local_ctx();

        if (onvm_nflib_start_nf(child_local_ctx, child_init_cfg) < 0) {
                printf("Failed to spawn child NF\n");
                return NULL;
        }

        /* Keep track of parent for proper termination */
        child_local_ctx->nf->thread_info.parent = parent->instance_id;

        tx_loop(child_local_ctx);
        onvm_nflib_stop(child_local_ctx);
        free(spawn_info);
        return NULL;
}

struct token_bucket_config *
parse_config_file(uint8_t *num_queues, char *config_file) {
        uint8_t i = 0;
        if (config_file == NULL) {
                /* Default Configuration of the fair queue system */
                *num_queues = 2;
                config = (struct token_bucket_config *)malloc(2 * sizeof(struct token_bucket_config));
                for (; i < *num_queues; i++) {
                        config[i].rate = 1000;
                        config[i].depth = 10000;
                }
        } else {
                cJSON *config_params = onvm_config_parse_file(config_file);
                if (config_params == NULL) {
                        rte_exit(EXIT_FAILURE,
                                 "%s file could not be parsed/not found. Assure config file"
                                 " the directory to the config file is being specified.\n",
                                 config_file);
                }

                cJSON *num_q = NULL;

                num_q = cJSON_GetObjectItem(config_params, "num_queues");
                if (num_q == NULL)
                        rte_exit(EXIT_FAILURE, "num_queues not found/invalid\n");
                *num_queues = num_q->valueint;

                config = (struct token_bucket_config *)malloc(num_q->valueint * sizeof(struct token_bucket_config));

                cJSON *queue_list = NULL;
                cJSON *rate = NULL;
                cJSON *depth = NULL;
                queue_list = cJSON_GetObjectItem(config_params, "queues");
                if (queue_list == NULL)
                        rte_exit(EXIT_FAILURE, "queues not found/invalid\n");
                int num_of_queues = onvm_config_get_item_count(queue_list);
                if (num_of_queues != num_q->valueint)
                        rte_exit(EXIT_FAILURE, "Not enough queue configurations\n");

                cJSON *queue = queue_list->child;
                while (queue) {
                        rate = cJSON_GetObjectItem(queue, "rate");
                        depth = cJSON_GetObjectItem(queue, "depth");

                        if (rate == NULL)
                                rte_exit(EXIT_FAILURE, "rate not found/invalid\n");
                        if (depth == NULL)
                                rte_exit(EXIT_FAILURE, "depth not found/invalid\n");

                        config[i].rate = rate->valueint;
                        config[i].depth = depth->valueint;

                        queue = queue->next;
                        i++;
                }
                cJSON_Delete(config_params);
        }
        return config;
}

int
main(int argc, char *argv[]) {
        pthread_t tx_thread;
        pthread_t stats_thread;
        struct onvm_nf_local_ctx *nf_local_ctx;
        struct onvm_nf_function_table *nf_function_table;
        struct onvm_nf *nf;
        int arg_offset;
        uint8_t num_queues;

        const char *progname = argv[0];

        nf_local_ctx = onvm_nflib_init_nf_local_ctx();

        /* If we're using advanced rings also pass a custom cleanup function,
         * this can be used to handle NF specific (non onvm) cleanup logic */
        rte_atomic16_init(&signal_exit_flag);
        rte_atomic16_set(&signal_exit_flag, 0);
        onvm_nflib_start_signal_handler(nf_local_ctx, sig_handler);

        /* No need to define a function table as adv rings won't run onvm_nflib_run */
        nf_function_table = NULL;

        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG, nf_local_ctx, nf_function_table)) < 0) {
                onvm_nflib_stop(nf_local_ctx);
                if (arg_offset == ONVM_SIGNAL_TERMINATION) {
                        printf("Exiting due to user termination\n");
                        return 0;
                } else {
                        rte_exit(EXIT_FAILURE, "Failed ONVM init\n");
                }
        }

        argc -= arg_offset;
        argv += arg_offset;

        if (parse_app_args(argc, argv, progname) < 0) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        }

        nf = nf_local_ctx->nf;
        struct onvm_nf_init_cfg *child_cfg;
        child_cfg = onvm_nflib_init_nf_init_cfg(nf->tag);

        /* Prepare init data for the child */
        child_cfg->service_id = nf->service_id;
        struct child_spawn_info *child_data = malloc(sizeof(struct child_spawn_info));
        child_data->child_cfg = child_cfg;
        child_data->parent = nf;

        /* Increment the children count so that stats are displayed and NF does proper cleanup */
        rte_atomic16_inc(&nf->thread_info.children_cnt);

        /* Parse the config file to extract the fair queue configuration */
        config = parse_config_file(&num_queues, config_file);

        /* Set up fair_queue */
        setup_fairq(&fairq, num_queues, config);

        pthread_create(&tx_thread, NULL, start_child, (void *)child_data);
        pthread_create(&stats_thread, NULL, do_fq_stats_display, NULL);

        rx_loop(nf_local_ctx);
        onvm_nflib_stop(nf_local_ctx);

        pthread_join(tx_thread, NULL);
        pthread_join(stats_thread, NULL);

        free(config);
        destroy_fairq(fairq);

        printf("If we reach here, program is ending\n");
        return 0;
}
