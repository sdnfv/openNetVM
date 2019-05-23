#ifndef _HISTOGRAM_H_

#define _HISTOGRAM_H_ 

/************************************API**************************************/
#define HIST_BUCKETS 100

#define HIST_RATE_MAX_VAL  14800000

#define HIST_COST_MAX_VAL  500

struct histogram {

        uint32_t data[HIST_BUCKETS];
        uint32_t bucket_size;
        uint32_t max;       //highest value for histogram
        uint32_t min;       //lowest value for histogram

        uint32_t max_val;   //highest value present in the histogram
        uint32_t min_val;   //least value present in the histogram

        uint32_t total_count;   //count of all stores in the histogram (total entries in all buckets
        uint32_t running_avg;   //running avergage of all values inserted in the histogram
};
typedef struct histogram histogram_t;
void hist_init(volatile struct histogram *h, uint32_t max, uint32_t min);
void hist_clear(volatile struct histogram *h);
void hist_store(volatile struct histogram *h, uint32_t val);
void hist_print(volatile struct histogram *h);

uint32_t hist_mean(volatile struct histogram *h);
uint32_t hist_median(volatile struct histogram *h);
uint32_t hist_mode(volatile struct histogram *h);


#define MAX_HISTOGRAM_SAMPLES (100)
#define MIN_SAMPLES_FOR_HISTOGRAM (10)
#define DISCARD_INITIAL_SAMPLES_COUNT  (10)
//#define RESET_HISTOGRAM_EVERY_MAX_SAMPLES
//#define RESET_HISTOGRAM_EVERY_MAX_SAMPLES_CONDITIONALLY
typedef struct histogram_v2 {
        uint32_t val[MAX_HISTOGRAM_SAMPLES];    //raw values
        uint32_t cur_index;     //index to write current entry
        uint32_t min_val;       //min value in the val[]
        uint32_t max_val;       //max value in the val[]
        uint32_t running_avg;   //running avergage of all values inserted in the histogram
        uint32_t ewma_avg;      //EWMA at alpha = 0.25

        histogram_t histogram;  //actual histogram
        uint8_t is_initialized; //flag status indicating histogram created or not
        uint32_t mean_val;      //computed mean
        uint32_t median_val;    //computed median
        uint32_t mode_val;      //computed mode
        uint32_t perctl_val;    //percential value
        uint32_t discard_mode;  //discard samples until DISCARD_INITIAL_SAMPLES_COUNT

}histogram_v2_t;
typedef enum HIST_VAL_TYPE {
        VAL_TYPE_RUNNING_AVG =0,
        VAL_TYPE_MEAN =1,
        VAL_TYPE_MEDIAN =2,
        VAL_TYPE_MODE =3,
        VAL_TYPE_ABS_MIN  =4,
        VAL_TYPE_ABS_MAX =5,
        VAL_TYPE_25_PERCENTILE=6,
        VAL_TYPE_50_PERCENTILE=VAL_TYPE_MEDIAN,
        VAL_TYPE_75_PERCENTILE=7,
        VAL_TYPE_90_PERCENTILE=8,
        VAL_TYPE_99_PERCENTILE=9,
}HIST_VAL_TYPE_E;

void hist_init_v2(histogram_v2_t *h);
void hist_store_v2(histogram_v2_t *h, uint32_t val);
void hist_compute_v2(histogram_v2_t *h);
void hist_extract_all_v2(histogram_v2_t *h);
uint32_t hist_extract_v2(histogram_v2_t *h, HIST_VAL_TYPE_E val_type);
void hist_print_v2(histogram_v2_t *h);

uint32_t hist_percentile(volatile struct histogram *h, HIST_VAL_TYPE_E pt_type);

//missc useful functions.. not used;
uint32_t get_modes(uint32_t mode[], uint32_t histogram[], uint32_t limit);
uint32_t get_histogram(uint32_t histogram[], uint32_t limit,uint32_t value[], uint32_t count);

/************************************API**************************************/

/***************************Internal Functions********************************/

/***************************Internal Functions********************************/
#endif
