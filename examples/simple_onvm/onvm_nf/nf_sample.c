/*********************************************************************
 *                     openNetVM
 *       https://github.com/sdnfv/openNetVM
 *
 *  Copyright 2015 George Washington University
 *            2015 University of California Riverside
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 * nf_sample.c - an exemple using onvm_nflib. Print a message each p
 * package received
 ********************************************************************/

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include <rte_common.h>
#include <rte_mbuf.h>

#include "onvm_nflib.h"

/* number of package between each print */
static uint8_t print_delay;

/* our client id number - tells us which rx queue to read, and NIC TX
 * queue to write to. */
static uint8_t client_id;

/*
 * print a usage message
 */
static void
usage(const char *progname)
{
        printf("Usage: %s [EAL args] -- [NF_LIB args] -p <print_delay>\n\n", progname);
}


/*
 * Convert the a string to an int.
 */
static int
parse_string(const char *client)
{
        char *end = NULL;
        unsigned long temp;

        if (client == NULL || *client == '\0')
                return -1;

        temp = strtoul(client, &end, 10);
        if (end == NULL || *end != '\0')
                return -1;

        client_id = (uint8_t)temp;
        return 0;
}

/*
 * Parse the application arguments to the client app.
 */
static int
parse_app_args(int argc, char *argv[])
{
        int option_index, opt;

        char **argvopt = argv;
        const char *progname = NULL;
        static struct option lgopts[] = { /* no long options */
                {NULL, 0, 0, 0 }
        };
        progname = argv[0];

        while ((opt = getopt_long(argc, argvopt, "p:", lgopts,
                &option_index)) != EOF){
                switch (opt){
                        case 'p':
                                if (parse_string(optarg) < 1){
                                        usage(progname);
                                        return -1;
                                }
                                print_delay = parse_string(optarg);
                                break;
                        default:
                                usage(progname);
                                return -1;
                }
        }
        return 0;
}



static void
packet_handler(struct rte_mbuf* pkt) {
        static int counter = 0;

        if(counter++ == print_delay) {
                printf("pkt on port %d and size %d\n" , pkt->port , pkt->pkt_len);
                counter = 0;
        }
        onvm_nf_return_packet(pkt, ONVM_NF_ACTION_OUT, 0);
}


int main(int argc, char *argv[]) {
        struct onvm_nf_info info;
        int retval;

        if ((retval = onvm_nf_init(argc, argv, &info)) < 0)
                return -1;
        argc -= retval;
        argv += retval;

        if (parse_app_args(argc, argv) < 0)
                exit(EXIT_FAILURE);

        onvm_nf_run(&info, &packet_handler);
        printf("If we reach here, program is ending");
        return 0;
}
