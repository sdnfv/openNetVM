#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "histogram.h"

#define MIN(a,b) ((a) < (b)? (a):(b))
#define MAX(a,b) ((a) > (b)? (a):(b))
#define MIN0(a,b) ((a==0)?(b):MIN(a,b))
#define RUN_AVG(a,b) ((a==0)?(b):((a+b)/2))
#define EWMA_ALPHA  (0.25)
#define EWMA_AVG(a,b) ((a==0)?(b):( ( (a*(1-EWMA_ALPHA)) + (b*EWMA_ALPHA)/2) ))

void hist_init(volatile struct histogram *h, uint32_t max, uint32_t min) {

        int i;
        for (i = 0; i < HIST_BUCKETS; i++) {
                h->data[i] = 0;
        }
        min = MIN(max,min);
        h->bucket_size = (max-min) / HIST_BUCKETS;

        if(0 == h->bucket_size) {
                printf("Invalid Bucket Size! min[%d], max[%d]", min,max);
                max = min + 2*HIST_BUCKETS;
                h->bucket_size = 2;
                //exit(0);
        }
        h->max = max;
        h->min = min;

        h->min_val = 0; //INT_MAX;
        h->max_val = 0;
        h->total_count=0;
        h->running_avg=0;

}

void hist_clear(volatile struct histogram *h) {
        hist_init(h,0,0);
}

void hist_store(volatile struct histogram *h, uint32_t val) {

        if(h && h->bucket_size == 0) return ;
        /* Store val into a histogram array.
         * Track the min and max value seen.
         */
        int index = 0 ;
        if(val < h->min) index = 0;
        else if(val > h->max) index = HIST_BUCKETS - 1;
        else index = (val - h->min)/h->bucket_size; //(val - h->min) * HIST_BUCKETS / (h->max -val);

        h->max_val = MAX(h->max_val, val);
        h->min_val = MIN0(h->min_val,val);
        h->running_avg = RUN_AVG(h->running_avg, val);

#ifndef USE_HISTOGRAM_AS_LIB
        printf("[V:%d, A:%d]\n", val, h->running_avg);
#endif
        h->data[index]++;
        h->total_count++;
}

void hist_print(volatile struct histogram *h) {

        int i;
        printf("\n min:%u ", h->min_val);
        printf("max:%u \n", h->max_val);
        //fprintf(stderr, "-min:%zu \n", h->min_val);
        //fprintf(stderr, "-max:%zu \n", h->max_val);
        for (i = 0; i < HIST_BUCKETS; i++) {
                //fprintf(stderr, "(%zu-%zu)%zu ", h->bucket_size*i, h->bucket_size*(i+1),h->data[i]);
                printf("(%u-%u):%u \n", (h->min + h->bucket_size * i),
                                (h->min + h->bucket_size * (i + 1)), h->data[i]);

        }
        printf("\n\n");
        printf("Running Avg:[%d], Mean:[%d], Median:[%d], Mode[%d]\n", h->running_avg, hist_mean(h), hist_median(h), hist_mode(h));
        //fprintf(stderr, "\n\n");
}

uint32_t hist_mean(volatile struct histogram *h) {

        int i;
        uint16_t count = 0;
        uint32_t total = 0;
        uint32_t avg = 0;
        for (i = 0; i < HIST_BUCKETS; i++) {
                total += ((h->min + ( (h->bucket_size * i + h->bucket_size * (i + 1)) / 2))* h->data[i]);
                count += h->data[i];
        }

        if (count != 0) {
                avg = total/count;
        }
        return avg;

}
uint32_t hist_median(volatile struct histogram *h) {
        int i;
        uint32_t total = 0;
        uint32_t median = 0;
/* Given by formula Lm + { [ N/2 - F(m-1)] /(fm)}
//where Lm = Lower limit of median bar
        N = Total observations
        F(m-1) = total cumulative frequency of all preceeding bars of median (uptill median bar).
        fm = frequency of median bar
*/
/*
        uint16_t count = 0;
        uint32_t lm = 0;
        uint32_t cfp = 0;


        for (i = 0; i < HIST_BUCKETS; i++) {

                total += h->data[i];

                if(i < HIST_BUCKETS/2)
                        cfp += h->data[i];

                if(i == HIST_BUCKETS/2) {
                        lm =  h->bucket_size * i;
                        count = h->data[i];
                }

        }
        median = lm;
        if (count != 0) {
                median += (( ( (total/2) - cfp) )/(count));

        }
        */

        //evaluate approximate median of histogram as numb at mid range
        for (i = 0; i < HIST_BUCKETS; i++) {
                if((total > ((h->total_count - h->data[i])/2))) break;
                total +=  h->data[i];
        }

        median = (h->min + ((h->bucket_size * i + h->bucket_size * (i+1))/2));
        return median;
}

uint32_t hist_mode(volatile struct histogram *h) {
//mode = avg value of bucket with highest hits..

        int i;
        uint16_t max_index = 0;
        uint32_t max_val = 0;
        uint32_t mode = 0;

        for (i = 0; i < HIST_BUCKETS; i++) {
                if( h->data[i] > max_val) {
                        max_index = i;
                        max_val = h->data[i];
                }
        }
        mode = (h->min + ((h->bucket_size * max_index + h->bucket_size * (max_index + 1)) / 2));
        return mode;
}
uint32_t hist_percentile(volatile struct histogram *h, HIST_VAL_TYPE_E pt_type) {
        uint32_t pt_val = 0;
        uint32_t total = 0;
        uint32_t i = 0;
        switch(pt_type) {
        case VAL_TYPE_25_PERCENTILE:
                pt_val = (h->total_count*25)/100;
                break;
        case VAL_TYPE_50_PERCENTILE:
                pt_val = (h->total_count*50)/100;
                break;
        case VAL_TYPE_75_PERCENTILE:
                pt_val = (h->total_count*75)/100;
                break;
        case VAL_TYPE_90_PERCENTILE:
                pt_val = (h->total_count*90)/100;
                break;
        case VAL_TYPE_99_PERCENTILE:
                pt_val = (h->total_count*99)/100;
                break;
        default:
                return 0;
        }

        //evaluate percentile of histogram as numb at specified range
        for (i = 0; i < HIST_BUCKETS; i++) {
                if(total > pt_val) break;
                total +=  h->data[i];
        }

        pt_val = (h->min + ((h->bucket_size * i + h->bucket_size * (i+1))/2));
        return pt_val;
}

#if 0
//Other reference functions; not used;
/* Calculate histogram of value[0..count-1], for 0 <= value < limit.
 * The histogram[0..limit-1] is cleared to zero.
 * Returns the number of values between 0..limit-1
 * (which should be count, unless some values exceeded the range).
 */
uint32_t get_histogram(uint32_t histogram[], uint32_t limit,
                uint32_t value[], uint32_t count) {
        uint32_t n = count;
        uint32_t i;

        /* Clear histogram. The U is there just to remind you it's unsigned. */
        for (i = 0; i < limit; i++)
                histogram[i] = 0;

        /* Compute histogram. This one checks for limit. */
        for (i = 0; i < count; i++)
                if (value[i] < limit)
                        histogram[value[i]]++;
                else
                        n--;

        /* Return the number of values counted (sum of histogram). */
        return n;
}

/* Find the mode or modes in histogram,
 * copy them into mode[] array (up to limit values),
 * and return the number of modes found.
 */
uint32_t get_modes(uint32_t mode[], uint32_t histogram[],
                uint32_t limit) {
        uint32_t i = 0;
        uint32_t max = 0U;
        uint32_t modes = 0U;

        for (i = 0; i < limit; i++)
                if (histogram[i] > max) {

                        /* New histogram peak found. */
                        max = histogram[i];

                        /* This is the first mode (for this histogram max). */
                        modes = 0;
                        mode[modes++] = i;

                }
                else if (histogram[i] == max && max > 0U) {

                        /* Another mode for this histogram max. */
                        mode[modes++] = i;
                }

        /* Return the number of modes in mode[].
         * Note: if the histogram is empty, we'll return 0.
         */
        return modes;
        /* Note: If the above function returns > 0, then there is
         * at least one mode listed in mode[].
         * To find the number of how many times that mode occurred,
         * use histogram[mode[0]].
         * If there are more than one mode in mode[],
         * they will all have the same histogram[] value
         * -- since they all occurred the same number of times.
         */
}
#endif

void hist_init_v2(histogram_v2_t *h) {
        int i;
        for (i = 0; i < MAX_HISTOGRAM_SAMPLES; i++) {
                h->val[i] = 0;

        }
        h->cur_index=0;
        h->is_initialized = 0;
        h->max_val = 0;
        h->min_val = 0;
        h->running_avg =0;

        h->mean_val = 0;
        h->median_val =0;
        h->mode_val = 0;

        h->discard_mode = 0;
}
void hist_store_v2(histogram_v2_t *h, uint32_t val) {

        if( h->discard_mode < DISCARD_INITIAL_SAMPLES_COUNT) {
                h->discard_mode++;
                return;
        }

        h->val[h->cur_index++] = val;
        h->max_val = MAX(h->max_val,val);
        h->min_val = MIN0(h->min_val,val);
        h->running_avg = RUN_AVG(h->running_avg,val);
        h->ewma_avg = EWMA_AVG(h->ewma_avg, val);

        if(h->cur_index == MAX_HISTOGRAM_SAMPLES) {

                #if defined (RESET_HISTOGRAM_EVERY_MAX_SAMPLES) && !defined(RESET_HISTOGRAM_EVERY_MAX_SAMPLES_CONDITIONALLY)
                h->is_initialized = 0;    //reset if we want to recreate the histogram for every 100 samples; better do conditionally, if values have dramatically changed.
                #endif //RESET_HISTOGRAM_EVERY_MAX_SAMPLES

                hist_compute_v2(h);
                h->cur_index = 0;
        }
}

void hist_compute_v2(histogram_v2_t *h) {
        if(!h) return;
        if((0 == h->is_initialized) && (MIN_SAMPLES_FOR_HISTOGRAM > h->cur_index)) return;
        if(0 == h->max_val || 0 == h->min_val) return;
        if(0 == h->is_initialized) {
                hist_init(&h->histogram,h->max_val+h->min_val/2, h->min_val);
                h->is_initialized = 1;
        } else {
                #if defined (RESET_HISTOGRAM_EVERY_MAX_SAMPLES_CONDITIONALLY ) //  && !defined(RESET_HISTOGRAM_EVERY_MAX_SAMPLES)
                if(( h->max_val && h->max_val > (h->histogram.max+h->histogram.min/2) )|| (h->min_val && h->min_val < (h->histogram.min/2))) {
                        hist_init(&h->histogram,h->max_val+h->min_val/2, h->min_val);
                        h->is_initialized = 1;
                }
                #endif //RESET_HISTOGRAM_EVERY_MAX_SAMPLES_CONDITIONALLY
        }
        uint32_t i=0;
        for(i=0; i< h->cur_index; i++) {
                hist_store(&h->histogram, h->val[i]);
        }
        h->cur_index = 0;
}

void hist_extract_all_v2(histogram_v2_t *h) {
        if(h->cur_index) {
                hist_compute_v2(h);
        }
        h->mean_val =  hist_mean(&h->histogram);
        h->median_val = hist_median(&h->histogram);
        h->mode_val = hist_mode(&h->histogram);
        return;
}
uint32_t hist_extract_v2(histogram_v2_t *h, HIST_VAL_TYPE_E val_type) {

        switch (val_type) {
        case VAL_TYPE_ABS_MIN:
                return h->min_val;
                break;
        case VAL_TYPE_ABS_MAX:
                return h->max_val;
                break;
        default:
        case VAL_TYPE_RUNNING_AVG:
                return h->running_avg;
                break;
        case VAL_TYPE_MEAN:
                if(h->cur_index) {
                        //hist_extract_all_v2(h);
                        hist_compute_v2(h);
                }
                h->mean_val =  hist_mean(&h->histogram);
                return h->mean_val;
                break;
        case VAL_TYPE_MEDIAN:
                if(h->cur_index) {
                        //hist_extract_all_v2(h);
                        hist_compute_v2(h);
                }
                h->median_val = hist_median(&h->histogram);
                return h->median_val;
                break;
        case VAL_TYPE_MODE:
                if(h->cur_index) {
                        //hist_extract_all_v2(h);
                        hist_compute_v2(h);
                }
                h->mode_val = hist_mode(&h->histogram);
                return h->mode_val;
                break;
        case VAL_TYPE_25_PERCENTILE:
        case VAL_TYPE_75_PERCENTILE:
        case VAL_TYPE_90_PERCENTILE:
        case VAL_TYPE_99_PERCENTILE:
                return hist_percentile(&h->histogram, val_type);
                break;
        }
        return h->running_avg;
}

void hist_print_v2(histogram_v2_t *h) {
        hist_extract_all_v2(h);
        hist_print(&h->histogram);
        printf("Summary: Running Avg:[%d], Mean:[%d], Median:[%d], Mode[%d]\n", h->running_avg, h->mean_val, h->median_val, h->mode_val);
}

//#define USE_HISTOGRAM_AS_LIB
#ifndef USE_HISTOGRAM_AS_LIB
int main() {
        struct histogram ht;
        hist_init(&ht, 2500,2000);
        int i = 0, j =250, k=2000;
        for (i = 0; i < 500; i++) {
                uint32_t val = MIN(2500, i+j+k);
                hist_store(&ht, val);
                if (i && (i % 5 == 0)) {
                        j += 50;
                        k -=5;
                }
                //printf("%d, ",val);
        }
        hist_print(&ht);
        //return 0;


        histogram_v2_t h2;
        hist_init_v2(&h2);
        i = 0, j =250, k=2000;
        for (i = 0; i < 500; i++) {
                uint32_t val = MIN(2500, i+j+k);
                hist_store_v2(&h2, val);
                if (i && (i % 5 == 0)) {
                        j += 50;
                        k +=5;
                }
                //printf("%d, ",val);
        }
        hist_extract_all_v2(&h2);
        hist_print_v2(&h2);
        return 0;
}
#endif //USE_HISTOGRAM_AS_LIB
