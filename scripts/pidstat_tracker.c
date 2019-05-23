#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct peformance_data_inputs {
        unsigned long int interval;
        unsigned long int repetition;
        char *core_list;
        char *outfile_path;
        char *nf_tags;
}pd_inputs_t;

const char *cmd = "pidstat -C \"bridge|forward\" -lrsuwh 1 30 | tee pidstat_log.csv";

const char *cmd2 = "perf stat --cpu=8,10 --per-core -r 30 -o pidstat2_log.csv sleep 1"; //-I 2000
//const char *cmd2 = "perf stat -B -e cache-references,cache-misses,cycles,instructions,branches,faults,migrations --cpu=8,10 --per-core -o pidstat2_log.csv sleep 10"; //-I 2000
//const char *cmd2 = "perf stat --cpu=8,10 --per-core -o pidstat2_log.csv sleep 30"; //-I 2000

int do_performance_log(void *pd_data) {
        /* ls -al | grep '^d' */
        FILE *pp=NULL, *pp2=NULL;
        //pp = popen("ls -al", "r");
        pp = popen(cmd, "r");
        pp2 = popen(cmd2, "r");
        int done = 0;
        if (pp != NULL && pp2 !=NULL) {
                while (1) {
                        char *line;
                        char buf[1000];
                        line = fgets(buf, sizeof buf, pp);
                        if (line == NULL) done |= 0x01; //break;
                        //if (line[0] == 'd') printf("%s", line); // line includes '\n'
                        //printf("%s", line);
                        
                        line = fgets(buf, sizeof buf, pp2);
                        if (line == NULL) done |= 0x02; //break;
                        //if (line[0] == 'd') printf("%s", line); // line includes '\n'
                        //printf("%s", line);
                        if(done == 0x03) break;
                }
                pclose(pp);
                pclose(pp2);
        }
        return 0;
}
int main(void) {
        void *pd_data = NULL;
        do_performance_log(pd_data);
}
