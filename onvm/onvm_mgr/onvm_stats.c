/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2016 George Washington University
 *            2015-2016 University of California Riverside
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
                          onvm_stats.c

   This file contain the implementation of all functions related to
   statistics display in the manager.

******************************************************************************/

#include <unistd.h>

#include "onvm_mgr.h"
#include "onvm_stats.h"
#include "onvm_nf.h"


/************************Internal Functions Prototypes************************/


/*
 * Function displaying statistics for all ports
 *
 * Input : time passed since last display (to compute packet rate)
 *
 */
static void
onvm_stats_display_ports(unsigned difftime);


/*
 * Function displaying statistics for all clients
 *
 */
static void
onvm_stats_display_clients(unsigned difftime);


/*
 * Function clearing the terminal and moving back the cursor to the top left.
 *
 */
static void
onvm_stats_clear_terminal(void);


/*
 * Function giving the MAC address of a port in string format.
 *
 * Input  : port
 * Output : its MAC address
 *
 */
static const char *
onvm_stats_print_MAC(uint8_t port);

/*
 * Flush the file streams we're writing stats to
 */
static void
onvm_stats_flush(void);

/*
 * Truncate the files back to being empty, so each iteration we reset the data
 */
static void
onvm_stats_truncate(void);

/*
 * Free and reset the cJSON variables to build new json string with
 */
static void
onvm_json_reset_objects(void);

/*********************Stats Output Streams************************************/

static FILE *stats_out;
static FILE *json_stats_out;

/****************************Interfaces***************************************/

void
onvm_stats_set_output(ONVM_STATS_OUTPUT output) {
        if (output != ONVM_STATS_NONE) {
                switch (output) {
                        case ONVM_STATS_STDOUT:
                                stats_out = stdout;
                                break;
                        case ONVM_STATS_STDERR:
                                stats_out = stderr;
                                break;
                        case ONVM_STATS_WEB:
                                stats_out = fopen(ONVM_STATS_FILE, ONVM_STATS_FOPEN_ARGS);
                                json_stats_out = fopen(ONVM_JSON_STATS_FILE, ONVM_STATS_FOPEN_ARGS);
                                onvm_json_root = NULL;
                                onvm_json_port_stats_arr = NULL;
                                onvm_json_nf_stats_arr = NULL;
                                break;
                        default:
                                rte_exit(-1, "Error handling stats output file\n");
                                break;
                }

                /* Ensure we're able to open all the files we need */
                if (stats_out == NULL || json_stats_out == NULL) {
                        rte_exit(-1, "Error opening stats files\n");
                }
        }
}

void
onvm_stats_cleanup(void) {
        if (stats_out != stdout && stats_out != stderr) {
                fclose(stats_out);
                fclose(json_stats_out);
        }
}

void
onvm_stats_display_all(unsigned difftime) {
        if (stats_out == stdout) {
                onvm_stats_clear_terminal();
        } else {
                onvm_stats_truncate();
                onvm_json_reset_objects();
        }

        onvm_stats_display_ports(difftime);
        onvm_stats_display_clients(difftime);

        if (stats_out != stdout && stats_out != stderr) {
                fprintf(json_stats_out, "%s\n", cJSON_Print(onvm_json_root));
        }

        onvm_stats_flush();
}


void
onvm_stats_clear_all_clients(void) {
        unsigned i;

        for (i = 0; i < MAX_CLIENTS; i++) {
                clients[i].stats.rx = clients[i].stats.rx_drop = 0;
                clients[i].stats.act_drop = clients[i].stats.act_tonf = 0;
                clients[i].stats.act_next = clients[i].stats.act_out = 0;
        }
}

void
onvm_stats_clear_client(uint16_t id) {
        clients[id].stats.rx = clients[id].stats.rx_drop = 0;
        clients[id].stats.act_drop = clients[id].stats.act_tonf = 0;
        clients[id].stats.act_next = clients[id].stats.act_out = 0;
}


/****************************Internal functions*******************************/


static void
onvm_stats_display_ports(unsigned difftime) {
        unsigned i = 0;
        uint64_t nic_rx_pkts = 0;
        uint64_t nic_tx_pkts = 0;
        uint64_t nic_rx_pps = 0;
        uint64_t nic_tx_pps = 0;
        char* port_label = NULL;
        /* Arrays to store last TX/RX count to calculate rate */
        static uint64_t tx_last[RTE_MAX_ETHPORTS];
        static uint64_t rx_last[RTE_MAX_ETHPORTS];

        fprintf(stats_out, "PORTS\n");
        fprintf(stats_out, "-----\n");
        for (i = 0; i < ports->num_ports; i++)
                fprintf(stats_out, "Port %u: '%s'\t", (unsigned)ports->id[i],
                                    onvm_stats_print_MAC(ports->id[i]));
        fprintf(stats_out, "\n\n");
        for (i = 0; i < ports->num_ports; i++) {
                nic_rx_pkts = ports->rx_stats.rx[ports->id[i]];
                nic_tx_pkts = ports->tx_stats.tx[ports->id[i]];

                nic_rx_pps = (nic_rx_pkts - rx_last[i]) / difftime;
                nic_tx_pps = (nic_tx_pkts - tx_last[i]) / difftime;

                fprintf(stats_out, "Port %u - rx: %9"PRIu64"  (%9"PRIu64" pps)\t"
                                "tx: %9"PRIu64"  (%9"PRIu64" pps)\n",
                                (unsigned)ports->id[i],
                                nic_rx_pkts,
                                nic_rx_pps,
                                nic_tx_pkts,
                                nic_tx_pps);

                /* Only print this information out if we haven't already printed it to the console above */
                if (stats_out != stdout && stats_out != stderr) {
                        ONVM_SNPRINTF(port_label, 8, "Port %d", i);

                        cJSON_AddItemToArray(onvm_json_port_stats_arr,
                                             onvm_json_port_stats[i] = cJSON_CreateObject());
                        cJSON_AddStringToObject(onvm_json_port_stats[i], "Label", port_label);
                        cJSON_AddNumberToObject(onvm_json_port_stats[i], "RX", nic_rx_pps);
                        cJSON_AddNumberToObject(onvm_json_port_stats[i], "TX", nic_tx_pps);

                        free(port_label);
                        port_label = NULL;
                }

                rx_last[i] = nic_rx_pkts;
                tx_last[i] = nic_tx_pkts;
        }
}


static void
onvm_stats_display_clients(unsigned difftime) {
        char* nf_label = NULL;
        unsigned i = 0;
        /* Arrays to store last TX/RX count for NFs to calculate rate */
        static uint64_t nf_tx_last[MAX_CLIENTS];
        static uint64_t nf_rx_last[MAX_CLIENTS];

        fprintf(stats_out, "\nCLIENTS\n");
        fprintf(stats_out, "-------\n");
        for (i = 0; i < MAX_CLIENTS; i++) {
                if (!onvm_nf_is_valid(&clients[i]))
                        continue;
                const uint64_t rx = clients[i].stats.rx;
                const uint64_t rx_drop = clients[i].stats.rx_drop;
                const uint64_t tx = clients_stats->tx[i];
                const uint64_t tx_drop = clients_stats->tx_drop[i];
                const uint64_t act_drop = clients[i].stats.act_drop;
                const uint64_t act_next = clients[i].stats.act_next;
                const uint64_t act_out = clients[i].stats.act_out;
                const uint64_t act_tonf = clients[i].stats.act_tonf;
                const uint64_t act_buffer = clients_stats->tx_buffer[i];
                const uint64_t act_returned = clients_stats->tx_returned[i];
                const uint64_t rx_pps = (rx - nf_rx_last[i])/difftime;
                const uint64_t tx_pps = (tx - nf_tx_last[i])/difftime;

                fprintf(stats_out, "Client %2u - rx: %9"PRIu64" rx_drop: %9"PRIu64" next: %9"PRIu64" drop: %9"PRIu64" ret: %9"PRIu64"\n"
                                   "            tx: %9"PRIu64" tx_drop: %9"PRIu64" out:  %9"PRIu64" tonf: %9"PRIu64" buf: %9"PRIu64" \n",
                                clients[i].info->instance_id,
                                rx, rx_drop, act_next, act_drop, act_returned,
                                tx, tx_drop, act_out, act_tonf, act_buffer);

                /* Only print this information out if we haven't already printed it to the console above */
                if (stats_out != stdout && stats_out != stderr) {
                        ONVM_SNPRINTF(nf_label, 6, "NF %d", i);

                        cJSON_AddItemToArray(onvm_json_nf_stats_arr,
                                             onvm_json_nf_stats[i] = cJSON_CreateObject());

                        cJSON_AddStringToObject(onvm_json_nf_stats[i],
                                                "Label", nf_label);
                        cJSON_AddNumberToObject(onvm_json_nf_stats[i],
                                                "RX", rx_pps);
                        cJSON_AddNumberToObject(onvm_json_nf_stats[i],
                                                "TX", tx_pps);

                        free(nf_label);
                        nf_label = NULL;
                }

                nf_rx_last[i] = clients[i].stats.rx;
                nf_tx_last[i] = clients_stats->tx[i];
        }

        fprintf(stats_out, "\n");
}


/***************************Helper functions**********************************/


static void
onvm_stats_clear_terminal(void) {
        const char clr[] = { 27, '[', '2', 'J', '\0' };
        const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };

        fprintf(stats_out, "%s%s", clr, topLeft);
}


static const char *
onvm_stats_print_MAC(uint8_t port) {
        static const char err_address[] = "00:00:00:00:00:00";
        static char addresses[RTE_MAX_ETHPORTS][sizeof(err_address)];

        if (unlikely(port >= RTE_MAX_ETHPORTS))
                return err_address;

        if (unlikely(addresses[port][0] == '\0')) {
                struct ether_addr mac;
                rte_eth_macaddr_get(port, &mac);
                snprintf(addresses[port], sizeof(addresses[port]),
                                "%02x:%02x:%02x:%02x:%02x:%02x\n",
                                mac.addr_bytes[0], mac.addr_bytes[1],
                                mac.addr_bytes[2], mac.addr_bytes[3],
                                mac.addr_bytes[4], mac.addr_bytes[5]);
        }

        return addresses[port];
}


static void
onvm_stats_flush(void) {
        if (stats_out == stdout || stats_out == stderr) {
                return;
        }

        fflush(stats_out);
        fflush(json_stats_out);
}


static void
onvm_stats_truncate(void) {
        if (stats_out == stdout || stats_out == stderr) {
                return;
        }

        stats_out = freopen(NULL, ONVM_STATS_FOPEN_ARGS, stats_out);
        json_stats_out = freopen(NULL, ONVM_STATS_FOPEN_ARGS, json_stats_out);

        /* Ensure we're able to open all the files we need */
        if (stats_out == NULL) {
                rte_exit(-1, "Error truncating stats files\n");
        }
}


static void
onvm_json_reset_objects(void) {
        if (onvm_json_root) {
                cJSON_Delete(onvm_json_root);
                onvm_json_root = NULL;
        }

        onvm_json_root = cJSON_CreateObject();

        cJSON_AddItemToObject(onvm_json_root, ONVM_JSON_PORT_STATS_KEY,
                              onvm_json_port_stats_arr = cJSON_CreateArray());
        cJSON_AddItemToObject(onvm_json_root, ONVM_JSON_NF_STATS_KEY,
                              onvm_json_nf_stats_arr = cJSON_CreateArray());
}
