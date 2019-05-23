#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "onvm_sort.h"

#define MIN(a,b) ((a) < (b)? (a):(b))
#define MAX(a,b) ((a) > (b)? (a):(b))
#define MIN0(a,b) ((a==0)?(b):MIN(a,b))
#define RUN_AVG(a,b) ((a==0)?(b):((a+b)/2))

int onvm_cmp_int_a(const void * a, const void *b) {
        int arg1 = *(const int*)a;
        int arg2 = *(const int*)b;
        int val = 0;

        if (arg1 < arg2) val = -1;
        else if (arg1 > arg2) val = 1;
        else    return 0;
        return val;
}
int onvm_cmp_int_d(const void * a, const void *b) {
        int arg1 = *(const int*)a;
        int arg2 = *(const int*)b;
        int val = 0;

        if (arg1 < arg2) val = 1;
        else if (arg1 > arg2) val = -1;
        else    return 0;
        return val;
}

void onvm_sort_int( void* ptr, ONVM_SORT_MODE_E sort_mode, size_t count,
                __attribute__((unused)) comp_func_s cf) {
        if(sort_mode == SORT_DESCENDING)
                qsort(ptr, count, sizeof(int), onvm_cmp_int_d); //qsort_r(ptr, count, sizeof(uint32_t), onvm_cmp_uint32_d, &sort_mode);
        else
                qsort(ptr, count, sizeof(int), onvm_cmp_int_a);   //qsort_r(ptr, count, sizeof(uint32_t), onvm_cmp_uint32_a, NULL);
}

int onvm_cmp_uint32_a(const void * a, const void *b) {
        uint32_t arg1 = *(const uint32_t*)a;
        uint32_t arg2 = *(const uint32_t*)b;
        int val = 0;

        if (arg1 < arg2) val = -1;
        else if (arg1 > arg2) val = 1;
        else    return 0;
        return val;
}
int onvm_cmp_uint32_d(const void * a, const void *b) {
        uint32_t arg1 = *(const uint32_t*)a;
        uint32_t arg2 = *(const uint32_t*)b;
        int val = 0;

        if (arg1 < arg2) val = 1;
        else if (arg1 > arg2) val = -1;
        else    return 0;
        return val;
}
void onvm_sort_uint32( void* ptr, ONVM_SORT_MODE_E sort_mode, size_t count,
                __attribute__((unused)) comp_func_s cf) {
        if(sort_mode == SORT_DESCENDING)
                qsort(ptr, count, sizeof(uint32_t), onvm_cmp_uint32_d); //qsort_r(ptr, count, sizeof(uint32_t), onvm_cmp_uint32_d, &sort_mode);
        else
                qsort(ptr, count, sizeof(uint32_t), onvm_cmp_uint32_a);   //qsort_r(ptr, count, sizeof(uint32_t), onvm_cmp_uint32_a, NULL);
}

int onvm_cmp_uint64_a(const void * a, const void *b) {
        uint64_t arg1 = *(const uint64_t*)a;
        uint64_t arg2 = *(const uint64_t*)b;
        int val = 0;

        if (arg1 < arg2) val = -1;
        else if (arg1 > arg2) val = 1;
        else    return 0;
        return val;
}
int onvm_cmp_uint64_d(const void * a, const void *b) {
        uint64_t arg1 = *(const uint64_t*)a;
        uint64_t arg2 = *(const uint64_t*)b;
        int val = 0;

        if (arg1 < arg2) val = 1;
        else if (arg1 > arg2) val = -1;
        else    return 0;
        return val;
}
void onvm_sort_uint64( void* ptr, ONVM_SORT_MODE_E sort_mode, size_t count,
                __attribute__((unused)) comp_func_s cf) {
        if(sort_mode == SORT_DESCENDING)
                qsort(ptr, count, sizeof(uint64_t), onvm_cmp_uint32_d); //qsort_r(ptr, count, sizeof(uint32_t), onvm_cmp_uint32_d, &sort_mode);
        else
                qsort(ptr, count, sizeof(uint64_t), onvm_cmp_uint32_a);   //qsort_r(ptr, count, sizeof(uint32_t), onvm_cmp_uint32_a, NULL);
}


int onvm_cmp_uint32_r(const void * a, const void *b, void* data) {
        uint32_t arg1 = *(const uint32_t*)a;
        uint32_t arg2 = *(const uint32_t*)b;
        int val = 0;

        if (arg1 < arg2) val = -1;
        else if (arg1 > arg2) val = 1;
        else    return 0;

        if(data) {
               switch(*((const ONVM_SORT_MODE_E*)data)) {
                       case SORT_ASCENDING:
                               return val;
                       case SORT_DESCENDING:
                               val *= (-1);
                               break;
                       default:
                               return val;
               }
        }
        return val;
}
int onvm_cmp_func_r (const void * a, const void *b, void* data) {
        if(data) {
               // onvm_callback_thunk_t *onvm_thunk = (onvm_callback_thunk_t*)data;
                return 0;
        }
        if(a && b) {
                return -1;
        }
        return 1;
}

#if 0
void onvm_sort_generic_r( void* ptr,  __attribute__((unused)) ONVM_SORT_DATA_TYPE_E data_type,  __attribute__((unused)) ONVM_SORT_MODE_E sort_mode, size_t count,
                __attribute__((unused)) size_t size, __attribute__((unused))  comp_func_s cf) {

        //stdlib has no support.
        if(cf) {
                qsort(ptr, count, size, cf);
        }
        else {
                //onvm_callback_thunk_t onvm_thunk = {.type=data_type, .mode=sort_mode, .cf=cf };
                switch(data_type) {
                case ONVM_SORT_TYPE_INT:
                        onvm_sort_int( ptr, sort_mode, count, NULL);
                        break;
                case ONVM_SORT_TYPE_UINT32:
                        onvm_sort_uint32( ptr, sort_mode, count, NULL);
                        break;
                case ONVM_SORT_TYPE_UINT64:
                        onvm_sort_uint64( ptr, sort_mode, count, NULL);
                        break;
                case ONVM_SORT_TYPE_CUSTOM:
                        break;
                }
                //qsort_r( ptr, count, size, onvm_cmp_func_s, &onvm_thunk);
                //qsort(ptr, count, size, onvm_cmp_uint32_a);
        }
}
#endif

void onvm_sort_generic( void* ptr, ONVM_SORT_DATA_TYPE_E data_type, ONVM_SORT_MODE_E sort_mode, size_t count,
                __attribute__((unused)) size_t size, __attribute__((unused)) comp_func cf) {
        if(cf) {
                qsort(ptr, count, size, cf);
        }
        else {
                switch(data_type) {
                case ONVM_SORT_TYPE_INT:
                        onvm_sort_int( ptr, sort_mode, count, NULL);
                        break;
                case ONVM_SORT_TYPE_UINT32:
                        onvm_sort_uint32( ptr, sort_mode, count, NULL);
                        break;
                case ONVM_SORT_TYPE_UINT64:
                        onvm_sort_uint64( ptr, sort_mode, count, NULL);
                        break;
                case ONVM_SORT_TYPE_CUSTOM:
                        break;
                }
        }
        return;

}

//#define USE_ONVM_SORT_AS_BIN
#ifdef USE_ONVM_SORT_AS_BIN
int main() {
        return 0;
}
#endif //USE_ONVM_SORT_AS_BIN
