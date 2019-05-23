#ifndef _ONVM_SORT_H_

#define _ONVM_SORT_H_

/************************************API**************************************/
typedef enum ONVM_SORT_DATA_TYPE {
        ONVM_SORT_TYPE_INT = 0,
        ONVM_SORT_TYPE_UINT32 = 1,
        ONVM_SORT_TYPE_UINT64 = 2,
        ONVM_SORT_TYPE_CUSTOM = 3,
}ONVM_SORT_DATA_TYPE_E;
typedef enum ONVM_SORT_MODE {
        SORT_ASCENDING =0,
        SORT_DESCENDING =1,
}ONVM_SORT_MODE_E;

typedef int (*comp_func)(const void *, const void *);
typedef int (*comp_func_s)(const void *, const void *, void*);

void onvm_sort_int( void* ptr, ONVM_SORT_MODE_E sort_mode, size_t count,
                __attribute__((unused)) comp_func_s cf);

void onvm_sort_uint32( void* ptr, ONVM_SORT_MODE_E sort_mode, size_t count,
                __attribute__((unused)) comp_func_s cf);

void onvm_sort_uint64( void* ptr, ONVM_SORT_MODE_E sort_mode, size_t count,
                __attribute__((unused)) comp_func_s cf);

void onvm_sort_generic( void* ptr, ONVM_SORT_DATA_TYPE_E data_type, ONVM_SORT_MODE_E sort_mode, size_t count,
                __attribute__((unused)) size_t size, __attribute__((unused)) comp_func cf);

void onvm_sort_generic_r( void* ptr, ONVM_SORT_DATA_TYPE_E data_type, ONVM_SORT_MODE_E sort_mode, size_t count,
                __attribute__((unused)) size_t size, __attribute__((unused)) comp_func_s cf);


//void onvm_sort_int(void);
//void onvm_sort_uint32_t(void);
//void onvm_sort_uint64_t(void);

typedef struct onvm_callback_thunk {
        ONVM_SORT_DATA_TYPE_E type;
        ONVM_SORT_MODE_E mode;
        comp_func_s cf;
}onvm_callback_thunk_t;

/************************************API**************************************/

/***************************Internal Functions********************************/

int onvm_cmp_func_r (const void * a, const void *b, void* data);
int onvm_cmp_int_r(const void * a, const void *b, void* data);
int onvm_cmp_int_a(const void * a, const void *b);
int onvm_cmp_int_d(const void * a, const void *b);
int onvm_cmp_uint32_r(const void * a, const void *b, void* data);
int onvm_cmp_uint32_a(const void * a, const void *b);
int onvm_cmp_uint32_d(const void * a, const void *b);
int onvm_cmp_uint64_r(const void * a, const void *b, void* data);
int onvm_cmp_uint64_a(const void * a, const void *b);
int onvm_cmp_uint64_d(const void * a, const void *b);



/***************************Internal Functions********************************/
#endif //_ONVM_SORT_H_
