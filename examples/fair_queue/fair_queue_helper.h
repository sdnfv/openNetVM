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
 * fair_queue_helper.h - functions and structures required to maintain
 *      multiple queue and simulate fair queueing
 ********************************************************************/

#include <onvm_flow_table.h>
#include <rte_cycles.h>
#include <rte_hash_crc.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_ring.h>

/* Token Bucket Configurations */
struct token_bucket_config {
        uint64_t rate;
        uint64_t depth;
};

/* Structure of each queue */
struct fq_queue {
        struct rte_ring *ring;                                  // rte ring structure
        struct rte_mbuf *head_pkt;                              // head packet from ring, used to know size of pkt
        uint64_t tb_tokens, tb_rate, tb_depth;                  // token bucket
        uint64_t last_cycle;                                    // last time tokens were generated for this queue
        uint64_t rx, rx_last, tx, tx_last;                      // maintaining stats
        uint64_t rx_drop, tx_drop, rx_drop_last, tx_drop_last;  // maintaining stats
};

/* Fairq structure */
struct fairq_t {
        struct fq_queue **fq;  // pointer to each queue
        uint8_t num_queues;    // number of queues
};

/*
 * Allocate memory to the fairq_t structure and initialize the variables along with token bucket configuration.
 */
static int
setup_fairq(struct fairq_t **fairq, uint8_t num_queues, struct token_bucket_config *config) {
        uint8_t i;
        uint64_t cur_cycles;
        char ring_name[4];

        *fairq = (struct fairq_t *)rte_malloc(NULL, sizeof(struct fairq_t), 0);
        if ((*fairq) == NULL) {
                rte_exit(EXIT_FAILURE, "Unable to allocate memory.\n");
        }

        (*fairq)->fq = (struct fq_queue **)rte_malloc(NULL, sizeof(struct fq_queue *) * num_queues, 0);
        if ((*fairq)->fq == NULL) {
                rte_exit(EXIT_FAILURE, "Unable to allocate memory for fair queue.\n");
        }

        (*fairq)->num_queues = num_queues;
        cur_cycles = rte_get_tsc_cycles();

        for (i = 0; i < num_queues; i++) {
                (*fairq)->fq[i] = (struct fq_queue *)rte_malloc(NULL, sizeof(struct fq_queue), 0);
                struct fq_queue *fq = (*fairq)->fq[i];

                snprintf(ring_name, 4, "fq%d", i);
                fq->ring = rte_ring_create(ring_name, NF_QUEUE_RINGSIZE / 4, rte_socket_id(),
                                           RING_F_SP_ENQ | RING_F_SC_DEQ);  // single producer, single consumer
                if (fq->ring == NULL) {
                        rte_exit(EXIT_FAILURE, "Unable to create ring for queue %d.\n",
                                 i + 1);  // TODO free previously allocated memory before exiting
                }
                fq->head_pkt = NULL;

                fq->tb_depth = config[i].depth;
                fq->tb_rate = config[i].rate;
                fq->tb_tokens = fq->tb_depth;

                fq->last_cycle = cur_cycles;

                fq->rx = 0;
                fq->rx_last = 0;
                fq->tx = 0;
                fq->tx_last = 0;
                fq->rx_drop = 0;
                fq->rx_drop_last = 0;
                fq->tx_drop = 0;
                fq->tx_drop_last = 0;
        }
        return 0;
}

/*
 * Free memory allocated to the fairq_t struct.
 */
static int
destroy_fairq(struct fairq_t *fairq) {
        uint8_t i;

        for (i = 0; i < fairq->num_queues; i++) {
                rte_ring_free(fairq->fq[i]->ring);
        }
        rte_free(fairq->fq);
        rte_free(fairq);
        return 0;
}

/*
 * Classify the incoming pkts based on the 5-tuple from the pkt header
 * (src IP, dst IP, src port, dst port, protocol)
 * into the queues of the fairq_t struct.
 * Internally called by `fairq_enqueue`.
 */
static int
get_enqueue_qid(struct fairq_t *fairq, struct rte_mbuf *pkt) {
        struct onvm_ft_ipv4_5tuple key;
        uint32_t hash_value;
        int ret;

        /* Obtain the 5tuple values */
        ret = onvm_ft_fill_key(&key, pkt);
        if (ret < 0) {
                printf("Received a non-IP4 packet!\n");
                return -1;
        }

        /* Classify pkts based on the 5tuple values */
        hash_value = fairq->num_queues;
        hash_value = rte_hash_crc_4byte(key.proto, hash_value);
        hash_value = rte_hash_crc_4byte(key.src_addr, hash_value);
        hash_value = rte_hash_crc_4byte(key.dst_addr, hash_value);
        hash_value = rte_hash_crc_4byte(key.src_port, hash_value);
        hash_value = rte_hash_crc_4byte(key.dst_port, hash_value);

        return hash_value % fairq->num_queues;
}

/*
 * Enqueue pkt to one of the queues of the fairq_t struct.
 */
static int
fairq_enqueue(struct fairq_t *fairq, struct rte_mbuf *pkt) {
        int qid;
        struct fq_queue *fq;

        qid = get_enqueue_qid(fairq, pkt);
        if (qid == -1) {
                return -1;
        }
        fq = fairq->fq[qid];
        if (rte_ring_enqueue(fq->ring, pkt) == 0) {
                fq->rx += 1;
        } else {
                /* ring was full */
                fq->rx_drop += 1;
                return -1;
        }

        return 0;
}

/*
 * Produce due tokens to a queue in the fairq_t struct.
 */
static int
produce_tokens(struct fq_queue *fq) {
        uint64_t cur_cycles;
        uint64_t timer_hz;
        uint64_t rate, time_diff;
        uint64_t tokens_produced;

        timer_hz = rte_get_timer_hz();
        rate = fq->tb_rate * 1000000;

        cur_cycles = rte_get_tsc_cycles();
        time_diff = cur_cycles - fq->last_cycle;

        /* Produce tokens upto maximum capacity */
        tokens_produced = ((time_diff * rate) / timer_hz);
        if (tokens_produced + fq->tb_tokens >= fq->tb_depth) {
                fq->tb_tokens = fq->tb_depth;
        } else {
                fq->tb_tokens += tokens_produced;
        }

        fq->last_cycle = cur_cycles;

        return 0;
}

/*
 * Check which queue has sufficient tokens in a round robin fashion.
 * Iterates through each queue atmost once and stops on finding a suitable queue.
 * Return the queue_id if any of the queues have sufficient tokens.
 * Return -1 otherwise.
 * Internally called by `fairq_dequeue`.
 */
static int
get_dequeue_qid(struct fairq_t *fairq) {
        static uint8_t qid = 0;
        uint64_t cur_cycles;
        uint64_t timer_hz;
        uint8_t start_qid;
        struct fq_queue *fq;

        start_qid = qid;
        do {
                fq = fairq->fq[qid];

                /* Check if pkt needs to be moved from ring to `head_pkt` */
                if (fq->head_pkt == NULL) {
                        if (rte_ring_count(fq->ring) == 0) {
                                qid = (qid + 1) % fairq->num_queues;
                                continue;
                        } else {
                                rte_ring_dequeue(fq->ring, (void **)&fq->head_pkt);
                        }
                }

                /* Check if queue has sufficient tokens */
                cur_cycles = rte_get_tsc_cycles();
                timer_hz = rte_get_timer_hz();
                if (fq->tb_tokens +
                        ((((cur_cycles - fq->last_cycle) * fq->tb_rate * 1000000) + timer_hz - 1) / timer_hz) >=
                        fq->head_pkt->pkt_len) {
                        produce_tokens(fq);

                        uint8_t temp_qid = qid;
                        qid = (qid + 1) % fairq->num_queues;
                        return temp_qid;
                }
                qid = (qid + 1) % fairq->num_queues;
        } while (qid != start_qid);  // Exit when none of the queue have sufficient tokens

        return -1;
}

/*
 * Dequeue pkt from the fairq_t queues in a round robin fashion.
 */
static int
fairq_dequeue(struct fairq_t *fairq, struct rte_mbuf **pkt) {
        int qid;
        struct fq_queue *fq;

        qid = get_dequeue_qid(fairq);

        if (qid == -1) {
                *pkt = NULL;
                return -1;
        }

        fq = fairq->fq[qid];

        /* Dequeue pkt */
        fq->tb_tokens -= fq->head_pkt->pkt_len;
        *pkt = fq->head_pkt;

        fq->tx += 1;

        /* Update `head_pkt` */
        if (rte_ring_dequeue(fq->ring, (void **)&fq->head_pkt) != 0) {
                fq->head_pkt = NULL;  // no more pkts in queue
        }

        return 0;
}