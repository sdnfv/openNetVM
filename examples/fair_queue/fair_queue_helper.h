#include <limits.h>
#include <onvm_flow_table.h>
#include <rte_cycles.h>
#include <rte_hash_crc.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_ring.h>
#include <stdlib.h>

struct token_bucket_config {
        uint64_t rate;
        uint64_t depth;
};

struct fq_queue {
        struct rte_ring *ring;                  // rte ring structure
        struct rte_mbuf *head_pkt;              // head packet from ring
        uint64_t tb_tokens, tb_rate, tb_depth;  // token bucket
        uint64_t last_cycle;                    // the last time tokens were generated
        uint64_t rx, rx_last, tx, tx_last;      // maintaining stats
        uint64_t rx_drop, tx_drop, rx_drop_last, tx_drop_last;
};

struct fairq_t {
        struct fq_queue **fq;  // pointer to each queue
        uint8_t n;             // number of queues 
};

static int
setup_fairq(struct fairq_t **fairq, uint8_t n, struct token_bucket_config *config) {
        uint8_t i;
	uint64_t cur_cycles;
        char ring_name[4];

        *fairq = (struct fairq_t *)rte_malloc(NULL, sizeof(struct fairq_t), 0);  // TODO: Check if memory was allocated
        (*fairq)->fq = (struct fq_queue **)rte_malloc(NULL, sizeof(struct fq_queue *) * n, 0);
        (*fairq)->n = n;
        cur_cycles = rte_get_tsc_cycles();

        for (i = 0; i < n; i++) {
                (*fairq)->fq[i] = (struct fq_queue *)rte_malloc(NULL, sizeof(struct fq_queue), 0);
                struct fq_queue *fq = (*fairq)->fq[i];

                snprintf(ring_name, 4, "fq%d", i);  // TODO: add checking
                fq->ring = rte_ring_create(ring_name, NF_QUEUE_RINGSIZE / 4,
                                           rte_socket_id(),                 // TODO: need to choose queue size carefully
                                           RING_F_SP_ENQ | RING_F_SC_DEQ);  // single producer, single consumer
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
                fq->tx_drop = 0;
                fq->rx_drop_last = 0;
                fq->tx_drop_last = 0;
        }

        return 0;
}

static int
destroy_fairq(struct fairq_t *fairq) {
        uint8_t i;
        for (i = 0; i < fairq->n; i++) {
                rte_ring_free(fairq->fq[i]->ring);
        }
        rte_free(fairq->fq);
        rte_free(fairq);
        return 0;
}

static int
get_fairq_id(struct fairq_t *fairq, struct rte_mbuf *pkt) {
        struct onvm_ft_ipv4_5tuple key;
        uint32_t key_sum;
        uint32_t hash_value;
        int ret = onvm_ft_fill_key(&key, pkt);
        if (ret < 0) {
                printf("Received a non-IP4 packet!\n");
                return -1;
        }
        key_sum = key.src_addr + key.dst_addr + key.src_port + key.dst_port + key.proto;
        hash_value = rte_hash_crc((void *)&key_sum, 32, fairq->n);
        return hash_value % fairq->n;
}

static int
fairq_enqueue(struct fairq_t *fairq, struct rte_mbuf *pkt) {
        int queue_id;
        struct fq_queue *fq;

        queue_id = get_fairq_id(fairq, pkt);
        if (queue_id == -1) {
                return -1;
        }
        fq = fairq->fq[queue_id];
        if (rte_ring_enqueue(fq->ring, pkt) == 0) {
                fq->rx += 1;
        } else {
                fq->rx_drop += 1;
                return -1;
        }

        return 0;
}

static int
produce_tokens(struct fq_queue *fq) {
        uint64_t tokens_produced;
        uint64_t timer_hz;
	uint64_t cur_cycles;
        timer_hz = rte_get_timer_hz();
        
	uint64_t rate = fq->tb_rate * 1000000;
        
	cur_cycles = rte_get_tsc_cycles();
        uint64_t time_diff = cur_cycles - fq->last_cycle;

        tokens_produced = ((time_diff * rate) / timer_hz);
        if (tokens_produced + fq->tb_tokens >= fq->tb_depth) {
                fq->tb_tokens = fq->tb_depth;
        } else {
                fq->tb_tokens += tokens_produced;
        }

        fq->last_cycle = cur_cycles;

        return 0;
}

static int
get_next_qid(struct fairq_t *fairq) {
        static uint8_t qid = 0;
	uint64_t cur_cycles;
        uint8_t start_qid = qid;
        do {
                struct fq_queue *fq = fairq->fq[qid];
                if (fq->head_pkt == NULL) {
                        if (rte_ring_count(fq->ring) == 0) {
                                qid = (qid + 1) % fairq->n;
                                continue;
                        } else {
                                rte_ring_dequeue(fq->ring, (void **)&fq->head_pkt);
                        }
                }

                cur_cycles = rte_get_tsc_cycles();
                uint64_t timer_hz;
                timer_hz = rte_get_timer_hz();
                if (fq->tb_tokens + ((((cur_cycles - fq->last_cycle) * fq->tb_rate * 1000000) + timer_hz - 1) / timer_hz) >= fq->head_pkt->pkt_len) {
                        produce_tokens(fq);
                        uint8_t temp_qid = qid;
                        qid = (qid + 1) % fairq->n;
                        return temp_qid;
                }
                qid = (qid + 1) % fairq->n;
        } while (qid != start_qid);

        return -1;
}

static int
fairq_dequeue(struct fairq_t *fairq, struct rte_mbuf **pkt) { 
        int dequeue_id = get_next_qid(fairq);

        if (dequeue_id == -1) {
                *pkt = NULL;
                return -1;
        }

        struct fq_queue *fq = fairq->fq[dequeue_id];

        // dequeue packet
        fq->tb_tokens -= fq->head_pkt->pkt_len;

        *pkt = fq->head_pkt;
        fq->tx += 1;
        if (rte_ring_dequeue(fq->ring, (void **)&fq->head_pkt) != 0) {
                fq->head_pkt = NULL;  // no more pkts in queue
        }

        return 0;
}
