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
 * args.c - argument processing for simple onvm
 ********************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <stdarg.h>
#include <errno.h>

#include <rte_memory.h>
#include <rte_string_fns.h>

#include "shared/common.h"
#include "onvm_mgr/args.h"
#include "onvm_mgr/init.h"

/* global var for number of clients - extern in header init.h */
uint8_t num_clients;

/* global var: did user directly specify num clients? */
uint8_t is_static_clients;

/* global var for program name */
static const char *progname;

/**
 * Prints out usage information to stdout
 */
static void
usage(void) {
        printf(
            "%s [EAL options] -- -p PORTMASK [-n NUM_CLIENTS] [-s NUM_SOCKETS]\n"
            " -p PORTMASK: hexadecimal bitmask of ports to use\n"
            " -n NUM_CLIENTS: number of client processes to use (optional)\n"
            , progname);
}

/**
 * The ports to be used by the application are passed in
 * the form of a bitmask. This function parses the bitmask
 * and places the port numbers to be used into the port[]
 * array variable
 */
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

/**
 * Take the number of clients parameter passed to the app
 * and convert to a number to store in the num_clients variable
 */
static int
parse_num_clients(const char *clients) {
        char *end = NULL;
        unsigned long temp;

        // If we want dynamic client numbering
        if (clients == NULL || *clients == '\0')
                return 0;

        temp = strtoul(clients, &end, 10);
        if (end == NULL || *end != '\0' || temp == 0)
                return -1;

        num_clients = (uint8_t)temp;
        is_static_clients = STATIC_CLIENTS;
        return 0;
}

/**
 * The application specific arguments follow the DPDK-specific
 * arguments which are stripped by the DPDK init. This function
 * processes these application arguments, printing usage info
 * on error.
 */
int
parse_app_args(uint8_t max_ports, int argc, char *argv[]) {
        int option_index, opt;
        char **argvopt = argv;
        static struct option lgopts[] = { /* no long options */
                {NULL, 0, 0, 0 }
        };
        progname = argv[0];
        is_static_clients = DYNAMIC_CLIENTS;

        while ((opt = getopt_long(argc, argvopt, "n:p:", lgopts,
                &option_index)) != EOF) {
                switch (opt) {
                        case 'p':
                                if (parse_portmask(max_ports, optarg) != 0) {
                                        usage();
                                        return -1;
                                }
                                break;
                        case 'n':
                                if (parse_num_clients(optarg) != 0) {
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
               && num_clients == 0) {
                usage();
                return -1;
        }

        return 0;
}
