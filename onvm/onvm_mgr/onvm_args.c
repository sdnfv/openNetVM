/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2017 George Washington University
 *            2015-2017 University of California Riverside
 *            2010-2014 Intel Corporation. All rights reserved.
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
 ********************************************************************/


/******************************************************************************

                                  onvm_args.c

    File containing the function parsing all DPDK and ONVM arguments.


******************************************************************************/


#include "onvm_mgr/onvm_args.h"
#include "onvm_mgr/onvm_stats.h"


/******************************Global variables*******************************/


/* global var for number of currently active NFs - extern in header init.h */
volatile uint16_t num_nfs = 0;

/* global var for number of services - extern in header init.h */
uint16_t num_services = MAX_SERVICES;

/* global var for the default service id - extern in init.h */
uint16_t default_service = DEFAULT_SERVICE_ID;

/* global var: did user directly specify num nfs? */
uint8_t is_static_clients;

/* global var to where to print stats - extern in init.h */
ONVM_STATS_OUTPUT stats_destination = ONVM_STATS_NONE;

/* global var for how long stats should wait before updating - extern in init.h */
uint16_t global_stats_sleep_time = 1;

/* global var for how verbose the stats output to console is - extern in init.h */
uint8_t global_verbosity_level = 1;

/* global var for program name */
static const char *progname;


/***********************Internal Functions prototypes*************************/


static void
usage(void);


static int
parse_portmask(uint8_t max_ports, const char *portmask);


static int
parse_default_service(const char *services);


static int
parse_num_services(const char *services);

#define USE_STATIC_IDS
#ifdef USE_STATIC_IDS
static int
parse_num_clients(const char *nfs);
#endif

static int
parse_stats_output(const char *stats_output);

static int
parse_stats_sleep_time(const char *sleeptime);

static int
parse_verbosity_level(const char *verbosity_level);

/*********************************Interfaces**********************************/


int
parse_app_args(uint8_t max_ports, int argc, char *argv[]) {
        int option_index, opt;
        char **argvopt = argv;

        static struct option lgopts[] = {
                {"port-mask",           required_argument,      NULL,   'p'},
                {"num-services",        required_argument,      NULL,   'r'},
                {"default-service",     required_argument,      NULL,   'd'},
                {"stats-out",           no_argument,            NULL,   's'},
                {"stats-sleep-time",    no_argument,            NULL,   'z'},
                {"verbocity-level",     no_argument,            NULL,   'v'}
        };

        progname = argv[0];
        is_static_clients = DYNAMIC_CLIENTS;

#ifdef USE_STATIC_IDS
        while ((opt = getopt_long(argc, argvopt, "n:r:p:d:s:z:v:", lgopts, &option_index)) != EOF) {
#else
        while ((opt = getopt_long(argc, argvopt, "r:p:d:s:z:v:", lgopts, &option_index)) != EOF) {
#endif
                switch (opt) {
                        case 'p':
                                if (parse_portmask(max_ports, optarg) != 0) {
                                        usage();
                                        return -1;
                                }
                                break;
#ifdef USE_STATIC_IDS
                        case 'n':
                                if (parse_num_clients(optarg) != 0) {
                                        usage();
                                        return -1;
                                }
                                break;
#endif
                        case 'r':
                                if (parse_num_services(optarg) != 0) {
                                        usage();
                                        return -1;
                                }
                                break;
                        case 'd':
                                if (parse_default_service(optarg) != 0) {
                                        usage();
                                        return -1;
                                }
                                break;
                        case 's':
                                if (parse_stats_output(optarg) != 0) {
                                        usage();
                                        return -1;
                                }

                                onvm_stats_set_output(stats_destination);
                                break;
                        case 'z':
                                if(parse_stats_sleep_time(optarg) != 0){
                                        usage();
                                        return -1;
                                }
                                break;
                        case 'v':
                                if(parse_verbosity_level(optarg) != 0){
                                        usage();
                                        return -1;
                                }
                                break;
                        default:
                                printf("ERROR: Unknown option '%c'\n", opt);
                                usage();
                                return -1;
                }
        }

        if (is_static_clients == STATIC_CLIENTS
               && num_nfs == 0) {
                usage();
                return -1;
        }
        return 0;
}


/*****************************Internal functions******************************/


static void
usage(void) {
        printf(
            "%s [EAL options] -- -p PORTMASK [-r NUM_SERVICES] [-d DEFAULT_SERVICE] [-s STATS_OUTPUT]\n"
            "\t-p PORTMASK: hexadecimal bitmask of ports to use\n"
            "\t-r NUM_SERVICES: number of unique serivces allowed. defaults to 16 (optional)\n"
            "\t-d DEFAULT_SERVICE: the service to initially receive packets. defaults to 1 (optional)\n"
            "\t-s STATS_OUTPUT: where to output manager stats (stdout/stderr/web). defaults to NONE (optional)\n"
            "\t-z STATS_SLEEP_TIME: how long the stats thread should wait before updating the stats (in seconds)\n"
            "\t-v VERBOCITY_LEVEL: verbocity level of the stats output(optional)\n"
#ifdef USE_STATIC_IDS
            " -n NUM_CLIENTS: number of client processes to use (optional)\n"
#endif
            , progname);
}


static int
parse_portmask(uint8_t max_ports, const char *portmask) {
        char *end = NULL;
        unsigned long pm;
        uint8_t count = 0;

        if (portmask == NULL)
                return -1;

        /* convert parameter to a number and verify */
        pm = strtoul(portmask, &end, 16);
        if (pm == 0) {
                printf("WARNING: No ports are being used.\n");
                return 0;
        }
        if (end == NULL || *end != '\0' || pm == 0)
                return -1;

        /* loop through bits of the mask and mark ports */
        while (pm != 0) {
                if (pm & 0x01) { /* bit is set in mask, use port */
                        if (count >= max_ports)
                                printf("WARNING: requested port %u not present"
                                " - ignoring\n", (unsigned)count);
                        else
                            ports->id[ports->num_ports++] = count;
                }
                pm = (pm >> 1);
                count++;
        }

        return 0;
}


static int
parse_default_service(const char *services) {
        char *end = NULL;
        unsigned long temp;

        temp = strtoul(services, &end, 10);
        if (end == NULL || *end != '\0' || temp == 0)
                return -1;

        default_service = (uint16_t)temp;
        return 0;
}


static int
parse_num_services(const char *services) {
        char *end = NULL;
        unsigned long temp;

        temp = strtoul(services, &end, 10);
        if (end == NULL || *end != '\0' || temp == 0)
                return -1;

        num_services = (uint16_t)temp;
        return 0;
}

static int
parse_stats_sleep_time(const char *sleeptime){
        char* end = NULL;
        unsigned long temp;

        temp = strtoul(sleeptime, &end, 10);
        if(end == NULL || *end != '\0' || temp == 0)
                return -1;

        global_stats_sleep_time = (uint16_t)temp;
        return 0;
}

static int
parse_stats_output(const char *stats_output) {
        if (!strcmp(stats_output, ONVM_STR_STATS_STDOUT)) {
                stats_destination = ONVM_STATS_STDOUT;
                return 0;
        } else if (!strcmp(stats_output, ONVM_STR_STATS_STDERR)) {
                stats_destination = ONVM_STATS_STDERR;
                return 0;
        } else if (!strcmp(stats_output, ONVM_STR_STATS_WEB)) {
                stats_destination = ONVM_STATS_WEB;
                return 0;
        } else {
                return -1;
        }
}
 
static int
parse_verbosity_level(const char *verbosity_level){
        char* end = NULL;
        unsigned long temp;

        temp = strtoul(verbosity_level, &end, 10);
        if(end == NULL || *end != '\0' || temp == 0)
                return -1;

        global_verbosity_level = (uint16_t)temp;
        return 0;
}

#ifdef USE_STATIC_IDS
static int
parse_num_clients(const char *nfs) {
        char *end = NULL;
        unsigned long temp;

        // If we want dynamic client numbering
        if (nfs == NULL || *nfs == '\0')
                return 0;

        temp = strtoul(nfs, &end, 10);
        if (end == NULL || *end != '\0' || temp == 0)
                return -1;

        num_nfs = (uint16_t)temp;
        is_static_clients = STATIC_CLIENTS;
        return 0;
}
#endif
