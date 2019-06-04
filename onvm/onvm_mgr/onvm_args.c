/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2019 George Washington University
 *            2015-2019 University of California Riverside
 *            2010-2019 Intel Corporation. All rights reserved.
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
uint16_t num_nfs;

/* global var for number of services - extern in header init.h */
uint16_t num_services = MAX_SERVICES;

/* global var for the default service id - extern in init.h */
uint16_t default_service = DEFAULT_SERVICE_ID;

/* global var to where to print stats - extern in init.h */
ONVM_STATS_OUTPUT stats_destination = ONVM_STATS_NONE;

/* global var for how long stats should wait before updating - extern in init.h */
uint16_t global_stats_sleep_time = 1;

/* global var for time_to_live, how long to wait until shutdown - extern in init.h */
uint32_t global_time_to_live = 0;

/* global var for time_to_live, how long to wait until shutdown - extern in init.h */
uint32_t global_pkt_limit = 0;

/* global var for how verbose the stats output to console is - extern in init.h */
uint8_t global_verbosity_level = 1;

/* global flag for enabling shared core logic - extern in init.h */
uint8_t ONVM_NF_SHARE_CORES = 0;

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

static int
parse_nf_cores(const char *services);

static int
parse_stats_output(const char *stats_output);

static int
parse_time_to_live(const char *time_to_live);

static int
parse_packet_limit(const char *pkt_limit);

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
            {"port-mask", required_argument, NULL, 'p'}, {"num-services", required_argument, NULL, 'r'},
            {"nf-cores", required_argument, NULL, 'n'},  {"default-service", required_argument, NULL, 'd'},
            {"stats-out", no_argument, NULL, 's'},       {"stats-sleep-time", no_argument, NULL, 'z'},
            {"time_to_live", no_argument, NULL, 't'},    {"packet_limit", no_argument, NULL, 'l'},
            {"verbocity-level", no_argument, NULL, 'v'}, {"enable_shared_cpu", no_argument, NULL, 'c'}};

        progname = argv[0];

        while ((opt = getopt_long(argc, argvopt, "p:r:n:d:s:t:l:z:v:c", lgopts, &option_index)) != EOF) {
                switch (opt) {
                        case 'p':
                                if (parse_portmask(max_ports, optarg) != 0) {
                                        usage();
                                        return -1;
                                }
                                break;
                        case 'r':
                                if (parse_num_services(optarg) != 0) {
                                        usage();
                                        return -1;
                                }
                                break;
                        case 'n':
                                if (parse_nf_cores(optarg) != 0) {
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
                        case 't':
                                if (parse_time_to_live(optarg) != 0) {
                                        usage();
                                        return -1;
                                }
                                break;
                        case 'l':
                                if (parse_packet_limit(optarg) != 0) {
                                        usage();
                                        return -1;
                                }
                                break;
                        case 'z':
                                if (parse_stats_sleep_time(optarg) != 0) {
                                        usage();
                                        return -1;
                                }
                                break;
                        case 'v':
                                if (parse_verbosity_level(optarg) != 0) {
                                        usage();
                                        return -1;
                                }
                                break;
                        case 'c':
                                onvm_config->flags.ONVM_NF_SHARE_CORES = 1;
                                ONVM_NF_SHARE_CORES = 1;
                                break;
                        default:
                                printf("ERROR: Unknown option '%c'\n", opt);
                                usage();
                                return -1;
                }
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
            "\t-t TTL: time to live, how many seconds to wait until exiting (optional)\n"
            "\t-l PACKET_LIMIT: how many millions of packets to recieve before exiting (optional)\n"
            "\t-v VERBOCITY_LEVEL: verbocity level of the stats output (optional)\n"
            "\t-c ENABLE_SHARED_CORE: allow the NFs to share a core based on mutex sleep/wakeups (optional)\n",
            progname);
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
                                printf(
                                    "WARNING: requested port %u not present"
                                    " - ignoring\n",
                                    (unsigned)count);
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
parse_nf_cores(const char *nf_coremask) {
        char *end = NULL;
        unsigned long pm;
        uint8_t i;
        uint8_t count = 0;
        uint8_t num_cores = 0;
        uint32_t max_cores = onvm_threading_get_num_cores();

        if (nf_coremask == NULL)
                return -1;

        /* convert parameter to a number and verify */
        pm = strtoul(nf_coremask, &end, 16);
        if (pm == 0) {
                printf("WARNING: No NF cores are being used.\n");
                printf("         Restart onvm_mgr with a valid coremask to run NFs.\n");
                return 0;
        }
        if (end == NULL || *end != '\0' || pm == 0)
                return -1;

        /* loop through bits of the mask and mark ports */
        while (pm != 0) {
                if (pm & 0x01) { /* bit is set in mask, use port */
                        if (count >= max_cores) {
                                printf(
                                    "WARNING: requested core %u out of cpu bounds"
                                    " - ignoring\n",
                                    (unsigned)count);
                        } else {
                                cores[count].enabled = 1;
                                cores[count].nf_count = 0;
                                num_cores++;
                        }
                }
                pm = (pm >> 1);
                count++;
                if (count == max_cores)
                        break;
        }

        count = 0;
        printf("Registered %d cores for NFs: ", num_cores);
        for (i = 0; i < max_cores; ++i) {
                if (cores[i].enabled == 1) {
                        printf("%d", i);
                        if (count != num_cores - 1)
                                printf(", ");
                        count++;
                }
        }
        printf("\n");

        return 0;
}

static int
parse_stats_sleep_time(const char *sleeptime) {
        char *end = NULL;
        unsigned long temp;

        temp = strtoul(sleeptime, &end, 10);
        if (end == NULL || *end != '\0' || temp == 0)
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
parse_time_to_live(const char *time_to_live) {
        char* end = NULL;
        unsigned long temp;

        temp = strtoul(time_to_live, &end, 10);
        if (end == NULL || *end != '\0' || temp == 0)
                return -1;

        global_time_to_live = (uint32_t)temp;
        return 0;
}

static int
parse_packet_limit(const char *pkt_limit) {
        char* end = NULL;
        unsigned long temp;

        temp = strtoul(pkt_limit, &end, 10);
        if (end == NULL || *end != '\0' || temp == 0)
                return -1;

        global_pkt_limit = (uint32_t)temp;
        return 0;
}

static int
parse_verbosity_level(const char *verbosity_level) {
        char *end = NULL;
        unsigned long temp;

        temp = strtoul(verbosity_level, &end, 10);
        if (end == NULL || *end != '\0' || temp == 0)
                return -1;

        global_verbosity_level = (uint16_t)temp;
        return 0;
}
