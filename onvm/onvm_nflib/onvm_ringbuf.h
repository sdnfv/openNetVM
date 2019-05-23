#ifndef _ONVM_SURING_H_

#define _ONVM_SURING_H_

/************************************API**************************************/
#define MAX_RINGBUF_SIZE    (512)
typedef struct onvm_single_user_ring {
        uint16_t r_count;           // num of entries in the ring[]
        uint16_t r_h;               // read_head in the ring[]
        uint16_t w_h;               // write head in the ring[]
        uint16_t max_len;           // Max size/count of ring[]
        void* ring[MAX_RINGBUF_SIZE];
}onvm_single_user_ring_t;

int onvm_su_ring_init(onvm_single_user_ring_t *pRingbuf, uint32_t max_size);
int onvm_su_ring_enqueu(onvm_single_user_ring_t *pRingbuf, void* pEntry);
int onvm_su_ring_dequeue(onvm_single_user_ring_t *pRingbuf, void **pEntry);
int onvm_su_ring_dinit(onvm_single_user_ring_t *pRingbuf);


/************************************API**************************************/

/***************************Internal Functions********************************/

/***************************Internal Functions********************************/
#endif //_ONVM_SURING_H_
