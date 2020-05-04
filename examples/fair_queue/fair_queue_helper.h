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
#include <rte_spinlock.h>

#define QUEUE_SIZE (NF_QUEUE_RINGSIZE)

/* Structure of each queue */
struct fairqueue_queue {
        uint32_t head;                                          // head of the queue
        uint32_t tail;                                          // tail of the queue
        struct rte_mbuf **pkts;                                 // array of packet pointers
        rte_spinlock_t lock;                                    // lock to access the queue
        uint64_t rx, rx_last, tx, tx_last;                      // maintaining stats
        uint64_t rx_drop, tx_drop, rx_drop_last, tx_drop_last;  // maintaining stats
};

/* Fair queue structure */
struct fairqueue_t {
        struct fairqueue_queue **fq;  // pointer to each queue
        uint16_t num_queues;          // number of queues
};

/*
 * Allocate memory to the fairqueue_t structure and initialize the variables.
 */
static int
setup_fairqueue(struct fairqueue_t **fairq, uint16_t num_queues) {
        uint16_t i;

        *fairq = (struct fairqueue_t *)rte_malloc(NULL, sizeof(struct fairqueue_t), 0);
        if ((*fairq) == NULL) {
                rte_exit(EXIT_FAILURE, "Unable to allocate memory.\n");
        }

        (*fairq)->fq = (struct fairqueue_queue **)rte_malloc(NULL, sizeof(struct fairqueue_queue *) * num_queues, 0);
        if ((*fairq)->fq == NULL) {
                rte_free(*fairq);
                rte_exit(EXIT_FAILURE, "Unable to allocate memory for fair queue.\n");
        }

        (*fairq)->num_queues = num_queues;

        for (i = 0; i < num_queues; i++) {
                (*fairq)->fq[i] = (struct fairqueue_queue *)rte_malloc(NULL, sizeof(struct fairqueue_queue), 0);
                if ((*fairq)->fq[i] == NULL) {
                        for (uint16_t j = 0; j < i; j++) {
                                rte_free((*fairq)->fq[j]);
                        }
                        rte_free((*fairq)->fq);
                        rte_free(*fairq);
                        rte_exit(EXIT_FAILURE, "Unable to allocate memory for queue %d.\n", i + 1);
                }
                struct fairqueue_queue *fq = (*fairq)->fq[i];

                fq->pkts = (struct rte_mbuf **)rte_malloc(NULL, sizeof(struct rte_mbuf *) * QUEUE_SIZE, 0);
                if (fq->pkts == NULL) {
                        for (uint16_t j = 0; j < i; j++) {
                                rte_free((*fairq)->fq[j]);
                        }
                        rte_free((*fairq)->fq[i]);
                        rte_free((*fairq)->fq);
                        rte_free(*fairq);
                        rte_exit(EXIT_FAILURE, "Unable to create queue for queue %d.\n", i + 1);
                }
                fq->head = 0;
                fq->tail = 0;
                rte_spinlock_init(&fq->lock);

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
 * Free memory allocated to the fairqueue_t struct.
 */
static int
destroy_fairqueue(struct fairqueue_t *fairq) {
        uint16_t i;

        for (i = 0; i < fairq->num_queues; i++) {
                rte_free(fairq->fq[i]->pkts);
                rte_free(fairq->fq[i]);
        }
        rte_free(fairq->fq);
        rte_free(fairq);
        return 0;
}

/*
 * Classify the incoming pkts based on the 5-tuple from the pkt header
 * (src IP, dst IP, src port, dst port, protocol)
 * into the queues of the fairqueue_t struct.
 * Internally called by `fairqueue_enqueue`.
 */
static int
get_enqueue_qid(struct fairqueue_t *fairq, struct rte_mbuf *pkt) {
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
 * Enqueue pkt to one of the queues of the fairqueue_t struct.
 */
static int
fairqueue_enqueue(struct fairqueue_t *fairq, struct rte_mbuf *pkt) {
        int qid;
        struct fairqueue_queue *fq;

        qid = get_enqueue_qid(fairq, pkt);
        if (qid == -1) {
                return -1;
        }
        fq = fairq->fq[qid];

        rte_spinlock_lock(&fq->lock);
        if (fq->head == fq->tail && fq->pkts[fq->head] != NULL) {
                fq->rx_drop += 1;
                return -1;
        }
        fq->pkts[fq->tail] = pkt;
        fq->tail = (fq->tail + 1) % QUEUE_SIZE;
        rte_spinlock_unlock(&fq->lock);
        
        fq->rx += 1;

        return 0;
}

/*
 * Iterates through each queue atmost once and stops on finding a non-empty queue.
 * Return the queue_id if queue isn't empty.
 * Return -1 otherwise.
 * Internally called by `fairqueue_dequeue`.
 */
static int
get_dequeue_qid(struct fairqueue_t *fairq) {
        static uint16_t qid = 0;
        uint16_t start_qid;
        struct fairqueue_queue *fq;

        start_qid = qid;
        do {
                fq = fairq->fq[qid];

                if (fq->pkts[fq->head] == NULL) {
                        qid = (qid + 1) % fairq->num_queues;
                        continue;
                }

                uint16_t temp_qid = qid;
                qid = (qid + 1) % fairq->num_queues;
                return temp_qid;
        } while (qid != start_qid);

        return -1;
}

/*
 * Dequeue pkt from the fairqueue_t queues in a round robin fashion.
 */
static int
fairqueue_dequeue(struct fairqueue_t *fairq, struct rte_mbuf **pkt) {
        int qid;
        struct fairqueue_queue *fq;

        qid = get_dequeue_qid(fairq);

        if (qid == -1) {
                *pkt = NULL;
                return -1;
        }

        fq = fairq->fq[qid];

        /* Dequeue pkt */
        rte_spinlock_lock(&fq->lock);
        *pkt = fq->pkts[fq->head];
        fq->pkts[fq->head] = NULL;
        fq->head = (fq->head + 1) % QUEUE_SIZE;
        rte_spinlock_unlock(&fq->lock);

        fq->tx += 1;

        return 0;
}
