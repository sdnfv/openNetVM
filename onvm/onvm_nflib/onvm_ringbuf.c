#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "onvm_ringbuf.h"

int onvm_su_ring_init(onvm_single_user_ring_t *pRingbuf, uint32_t max_size) {
        if(pRingbuf && max_size < MAX_RINGBUF_SIZE) {
                memset((void*)pRingbuf, 0, sizeof(onvm_single_user_ring_t));
                pRingbuf->max_len=max_size;
                return 0;
        }
        return -1;
}

int onvm_su_ring_enqueu(onvm_single_user_ring_t *pRingbuf, void* pEntry) {

        if(pRingbuf && pEntry) {
                if(((pRingbuf->w_h+1)%pRingbuf->max_len) == pRingbuf->r_h) {
                        return -1; //-EDQUOT;
                }
                pRingbuf->ring[pRingbuf->w_h]=pEntry;
                (pRingbuf->r_count)++;
                if((++(pRingbuf->w_h)) == pRingbuf->max_len) pRingbuf->w_h=0;
        }
        return -2;
}

int onvm_su_ring_dequeue(onvm_single_user_ring_t *pRingbuf, void **pEntry) {
        if(pRingbuf && pEntry) {
                if( pRingbuf->w_h == pRingbuf->r_h) {
                        *pEntry = 0;
                        return -1; //-EDQUOT; //empty
                }
                *pEntry = pRingbuf->ring[pRingbuf->r_h];
                if((++(pRingbuf->r_h)) == pRingbuf->max_len) pRingbuf->r_h=0;
                (pRingbuf->r_count)--;
        }
        return -2;
}

int onvm_su_ring_dinit(onvm_single_user_ring_t *pRingbuf) {
        return onvm_su_ring_init(pRingbuf,0);
}

//#define USE_ONVM_SURING_AS_BIN
#ifdef USE_ONVM_SURING_AS_BIN
//cc onvm_ringbuf.c -DUSE_ONVM_SURING_AS_BIN
int main() {
        onvm_single_user_ring_t my_ring;
        int i = onvm_su_ring_init(&my_ring, 50);
        for(i=0; i< 50;i++) {
                int *ptr = malloc(sizeof(int));
                *ptr = i*100;
                onvm_su_ring_enqueu(&my_ring, (void*)ptr);
        }
        for(i=0; i< 50;i++) {
                int *ptr = NULL;
                onvm_su_ring_dequeue(&my_ring, (void**)&ptr);
                if(ptr) {
                        printf("Dequeued:[%d]", *ptr);
                        free(ptr);
                }
        }
        onvm_su_ring_dinit(&my_ring);

        return 0;
}
#endif //USE_ONVM_SURING_AS_BIN
