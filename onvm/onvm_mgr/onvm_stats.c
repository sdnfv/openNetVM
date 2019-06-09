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
                          onvm_stats.c

   This file contain the implementation of all functions related to
   statistics display in the manager.

******************************************************************************/

#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "onvm_mgr.h"
#include "onvm_nf.h"
#include "onvm_stats.h"

/************************Internal Functions Prototypes************************/

/*
 * Function to send json events to web view
 *
 */
static void
onvm_stats_add_event(struct onvm_event *event_info);

/*
 * Function displaying statistics for all ports
 *
 * Input : time passed since last display (to compute packet rate)
 *
 */
static void
onvm_stats_display_ports(unsigned difftime, uint8_t verbosity_level);

/*
 * Function displaying statistics for all NFs
 *
 */
static void
onvm_stats_display_nfs(unsigned difftime, uint8_t verbosity_level);

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
static FILE *json_events_out;

/****************************Global variables***************************************/

/* Holds current timestamp, might want to make this not global */
char buffer[20];

/****************************Interfaces***************************************/

void
onvm_stats_init(uint8_t verbosity_level) {
        if (verbosity_level == ONVM_RAW_STATS_DUMP) {
                printf("%s", ONVM_STATS_RAW_DUMP_PORT_MSG);
                printf("%s", ONVM_STATS_RAW_DUMP_NF_MSG);
        }
}

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
                                json_events_out = fopen(ONVM_JSON_EVENTS_FILE, ONVM_STATS_FOPEN_ARGS);

                                if (stats_out == NULL || json_stats_out == NULL) {
                                        rte_exit(-1, "Error opening stats files\n");
                                }

                                onvm_json_root = NULL;
                                onvm_json_port_stats_obj = NULL;
                                onvm_json_nf_stats_obj = NULL;
                                onvm_json_events_arr = cJSON_CreateArray();
                                break;
                        default:
                                rte_exit(-1, "Error handling stats output file\n");
                                break;
                }
        }
}

void
onvm_stats_cleanup(void) {
        if (stats_destination == ONVM_STATS_WEB) {
                fclose(stats_out);
                fclose(json_stats_out);
                fclose(json_events_out);
                /* Delete all JSON objects to free memory */
                cJSON_Delete(onvm_json_root);
                cJSON_Delete(onvm_json_events_arr);
        }
}

void
onvm_stats_display_all(unsigned difftime, uint8_t verbosity_level) {
        time_t time_raw_format;
        struct tm *ptr_time;
        time(&time_raw_format);
        ptr_time = localtime(&time_raw_format);
        if (strftime(buffer, 20, "%F %T", ptr_time) == 0) {
                perror("Couldn't prepare formatted string");
        }

        if (stats_out == stdout) {
                if (verbosity_level != ONVM_RAW_STATS_DUMP)
                        onvm_stats_clear_terminal();
        } else {
                onvm_stats_truncate();
                onvm_json_reset_objects();
        }

        onvm_stats_display_ports(difftime, verbosity_level);
        onvm_stats_display_nfs(difftime, verbosity_level);

        if (stats_destination == ONVM_STATS_WEB) {
                fprintf(json_stats_out, "%s\n", cJSON_Print(onvm_json_root));
                fprintf(json_events_out, "%s\n", cJSON_Print(onvm_json_events_arr));
        }

        onvm_stats_flush();
}

void
onvm_stats_clear_all_nfs(void) {
        unsigned i;

        for (i = 0; i < MAX_NFS; i++) {
                onvm_stats_clear_nf(i);
        }
}

void
onvm_stats_clear_nf(uint16_t id) {
        nfs[id].stats.rx = nfs[id].stats.rx_drop = 0;
        nfs[id].stats.tx = nfs[id].stats.tx_drop = 0;
        nfs[id].stats.act_drop = nfs[id].stats.act_tonf = 0;
        nfs[id].stats.act_next = nfs[id].stats.act_out = 0;
        nfs[id].stats.tx_returned = nfs[id].stats.tx_buffer = 0;
}

void
onvm_stats_gen_event_info(const char *msg, uint8_t type, void *data) {
        struct onvm_event *event;

        event = (struct onvm_event *)malloc(sizeof(struct onvm_event));
        if (event == NULL) {
                perror("Couldn't allocate event");
                return;
        }

        event->type = type;
        event->msg = msg;
        event->data = data;

        onvm_stats_add_event(event);
}

void
onvm_stats_gen_event_nf_info(const char *msg, struct onvm_nf *nf) {
        struct onvm_event *event;

        event = (struct onvm_event *)malloc(sizeof(struct onvm_event));
        if (event == NULL) {
                perror("Couldn't allocate event");
                return;
        }

        event->type = ONVM_EVENT_NF_INFO;
        event->msg = msg;
        event->data = nf;

        onvm_stats_add_event(event);
}

/****************************Internal functions*******************************/

static void
onvm_stats_add_event(struct onvm_event *event_info) {
        if (event_info == NULL || stats_destination != ONVM_STATS_WEB) {
                return;
        }
        char event_time_buf[20];
        uint8_t type;
        struct tm *ptr_time;
        struct onvm_nf *nf;
        time_t time_raw_format;
        time(&time_raw_format);
        type = event_info->type;

        ptr_time = localtime(&time_raw_format);
        if (strftime(event_time_buf, 20, "%F %T", ptr_time) == 0) {
                perror("Couldn't prepare formatted string");
        }

        cJSON *source = cJSON_CreateObject();
        cJSON *new_event = cJSON_CreateObject();
        cJSON_AddStringToObject(new_event, "timestamp", event_time_buf);
        cJSON_AddStringToObject(new_event, "message", event_info->msg);

        if (type == ONVM_EVENT_WITH_CORE) {
                cJSON_AddNumberToObject(source, "core", *(int *)(event_info->data));
        } else if (type == ONVM_EVENT_PORT_INFO) {
                cJSON_AddStringToObject(source, "type", "MGR");
        } else if (type == ONVM_EVENT_NF_INFO) {
                nf = (struct onvm_nf *)event_info->data;
                if (nf->tag)
                        cJSON_AddStringToObject(source, "type", (char *)nf->tag);
                else
                        cJSON_AddStringToObject(source, "type", "NF");
                cJSON_AddNumberToObject(source, "instance_id", (int16_t)nf->instance_id);
                cJSON_AddNumberToObject(source, "service_id", (int16_t)nf->service_id);
                cJSON_AddNumberToObject(source, "core", (int16_t)nf->thread_info.core);
        } else if (type == ONVM_EVENT_NF_STOP) {
                cJSON_AddStringToObject(source, "type", "NF");
                cJSON_AddNumberToObject(source, "instance_id", *(int16_t *)(event_info->data));
        } else
                rte_exit(-1, "Invalid stats event type\n");

        cJSON_AddItemToObject(new_event, "source", source);
        cJSON_AddItemToArray(onvm_json_events_arr, new_event);
}

static void
onvm_stats_display_ports(unsigned difftime, uint8_t verbosity_level) {
        unsigned i = 0;
        uint64_t nic_rx_pkts = 0;
        uint64_t nic_tx_pkts = 0;
        uint64_t nic_rx_pps = 0;
        uint64_t nic_tx_pps = 0;
        char *port_label = NULL;
        /* Arrays to store last TX/RX count to calculate rate */
        static uint64_t tx_last[RTE_MAX_ETHPORTS];
        static uint64_t rx_last[RTE_MAX_ETHPORTS];
        static const char *PORT_MSG[3];

        PORT_MSG[0] = PORT_MSG[1] = "PORTS\n-----\n";
        PORT_MSG[2] = "";
        fprintf(stats_out, "%s", PORT_MSG[verbosity_level - 1]);

        if (verbosity_level != ONVM_RAW_STATS_DUMP) {
                for (i = 0; i < ports->num_ports; i++)
                        fprintf(stats_out, "Port %u: '%s'\t", (unsigned)ports->id[i],
                                onvm_stats_print_MAC(ports->id[i]));
                fprintf(stats_out, "\n\n");
        }
        for (i = 0; i < ports->num_ports; i++) {
                nic_rx_pkts = ports->rx_stats.rx[ports->id[i]];
                nic_tx_pkts = ports->tx_stats.tx[ports->id[i]];

                nic_rx_pps = (nic_rx_pkts - rx_last[i]) / difftime;
                nic_tx_pps = (nic_tx_pkts - tx_last[i]) / difftime;

                if (verbosity_level == ONVM_RAW_STATS_DUMP) {
                        fprintf(stats_out, ONVM_STATS_RAW_DUMP_PORTS_CONTENT, buffer,
                                (unsigned)ports->id[i], nic_rx_pkts, nic_rx_pps, nic_tx_pkts, nic_tx_pps);

                } else {
                        fprintf(stats_out, ONVM_STATS_REG_PORTS,
                                (unsigned)ports->id[i], nic_rx_pkts, nic_rx_pps, nic_tx_pkts, nic_tx_pps);
                }

                /* Only print this information out if we haven't already printed it to the console above */
                if (stats_out != stdout && stats_out != stderr) {
                        ONVM_SNPRINTF(port_label, 8, "Port %d", i);

                        cJSON_AddItemToObject(onvm_json_port_stats_obj, port_label,
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
onvm_stats_display_client_wakeup_thread_context(int difftime) {
        uint64_t num_wakeups = 0;
        uint64_t prev_num_wakeups = 0;
        uint64_t wakeup_rate;
        unsigned i = 0;

        for (i = 0; i < MAX_NFS; i++) {
                if (!onvm_nf_is_valid(&nfs[i]))
                        continue;
                num_wakeups += nf_wakeup_infos[i].num_wakeups;
                prev_num_wakeups += nf_wakeup_infos[i].prev_num_wakeups;
                nf_wakeup_infos[i].prev_num_wakeups = nf_wakeup_infos[i].num_wakeups;
        }

        wakeup_rate = (num_wakeups - prev_num_wakeups) / difftime;
        fprintf(stats_out, "Total wakeups = %"PRIu64", Wakeup rate = %"PRIu64"\n", num_wakeups, wakeup_rate);
}

static void
onvm_stats_display_nfs(unsigned difftime, uint8_t verbosity_level) {
        char *nf_label = NULL;
        unsigned i = 0;
        /* Arrays to store last TX/RX count for NFs to calculate rate */
        static uint64_t nf_tx_last[MAX_NFS];
        static uint64_t nf_rx_last[MAX_NFS];
        /* Arrays to store last TX/RX pkts dropped for NFs to calculate drop rate */
        static uint64_t nf_tx_drop_last[MAX_NFS];
        static uint64_t nf_rx_drop_last[MAX_NFS];
        static const char *NF_MSG[3];

        NF_MSG[0] = ONVM_STATS_MSG;
        if (ONVM_NF_SHARE_CORES) {
                NF_MSG[1] = ONVM_STATS_SHARED_CORE_MSG;
        } else {
                NF_MSG[1] = ONVM_STATS_ADV_MSG;
        }
        NF_MSG[2] = "";

        /* For same service id TX/RX stats */
        uint8_t print_total_stats = 0;
        uint64_t rx_for_service[MAX_SERVICES];
        uint64_t tx_for_service[MAX_SERVICES];
        uint64_t rx_drop_for_service[MAX_SERVICES];
        uint64_t tx_drop_for_service[MAX_SERVICES];
        uint64_t rx_pps_for_service[MAX_SERVICES];
        uint64_t tx_pps_for_service[MAX_SERVICES];
        uint64_t rx_drop_rate_for_service[MAX_SERVICES];
        uint64_t tx_drop_rate_for_service[MAX_SERVICES];
        uint64_t act_out_for_service[MAX_SERVICES];
        uint64_t act_tonf_for_service[MAX_SERVICES];
        uint64_t act_drop_for_service[MAX_SERVICES];
        uint64_t act_next_for_service[MAX_SERVICES];
        uint64_t act_buffer_for_service[MAX_SERVICES];
        uint64_t act_returned_for_service[MAX_SERVICES];
        for (i = 0; i < MAX_SERVICES; i++) {
                if (nf_per_service_count[i] > 1)
                        print_total_stats = 1;
                rx_for_service[i] = 0;
                tx_for_service[i] = 0;
                rx_drop_for_service[i] = 0;
                tx_drop_for_service[i] = 0;
                rx_pps_for_service[i] = 0;
                tx_pps_for_service[i] = 0;
                rx_drop_rate_for_service[i] = 0;
                tx_drop_rate_for_service[i] = 0;
                act_out_for_service[i] = 0;
                act_tonf_for_service[i] = 0;
                act_drop_for_service[i] = 0;
                act_next_for_service[i] = 0;
                act_buffer_for_service[i] = 0;
                act_returned_for_service[i] = 0;
        }

        fprintf(stats_out, "%s", NF_MSG[verbosity_level - 1]);
        for (i = 0; i < MAX_NFS; i++) {
                if (!onvm_nf_is_valid(&nfs[i]))
                        continue;
                const uint64_t rx = nfs[i].stats.rx;
                const uint64_t rx_drop = nfs[i].stats.rx_drop;
                const uint64_t tx = nfs[i].stats.tx;
                const uint64_t tx_drop = nfs[i].stats.tx_drop;
                const uint64_t act_out = nfs[i].stats.act_out;
                const uint64_t act_tonf = nfs[i].stats.act_tonf;
                const uint64_t act_drop = nfs[i].stats.act_drop;
                const uint64_t act_next = nfs[i].stats.act_next;
                const uint64_t act_buffer = nfs[i].stats.tx_buffer;
                const uint64_t act_returned = nfs[i].stats.tx_returned;
                const uint64_t rx_pps = (rx - nf_rx_last[i]) / difftime;
                const uint64_t tx_pps = (tx - nf_tx_last[i]) / difftime;
                const uint64_t tx_drop_rate = (tx_drop - nf_tx_drop_last[i]) / difftime;
                const uint64_t rx_drop_rate = (rx_drop - nf_rx_drop_last[i]) / difftime;
                const uint64_t num_wakeups = nf_wakeup_infos[i].num_wakeups;
                const uint64_t prev_num_wakeups = nf_wakeup_infos[i].prev_num_wakeups;
                const uint64_t wakeup_rate = (num_wakeups - prev_num_wakeups) / difftime;
                char state;

                uint8_t active = 0;
                if (ONVM_NF_SHARE_CORES)
                        active = rte_atomic16_read(nf_wakeup_infos[i].shm_server);
                if (!active) {
                        state = 'W';
                } else {
                        state = 'S';
                }

                /* Save stats for NFs with same service id */
                if (print_total_stats) {
                        rx_for_service[nfs[i].service_id] += rx;
                        tx_for_service[nfs[i].service_id] += tx;
                        rx_drop_for_service[nfs[i].service_id] += rx_drop;
                        tx_drop_for_service[nfs[i].service_id] += tx_drop;
                        rx_pps_for_service[nfs[i].service_id] += rx_pps;
                        tx_pps_for_service[nfs[i].service_id] += tx_pps;
                        rx_drop_rate_for_service[nfs[i].service_id] += rx_drop_rate;
                        tx_drop_rate_for_service[nfs[i].service_id] += tx_drop_rate;
                        act_out_for_service[nfs[i].service_id] += act_out;
                        act_tonf_for_service[nfs[i].service_id] += act_tonf;
                        act_drop_for_service[nfs[i].service_id] += act_drop;
                        act_next_for_service[nfs[i].service_id] += act_next;
                        act_buffer_for_service[nfs[i].service_id] += act_buffer;
                        act_returned_for_service[nfs[i].service_id] += act_returned;
                }

                if (verbosity_level == ONVM_RAW_STATS_DUMP) {
                        fprintf(stats_out, ONVM_STATS_RAW_DUMP_CONTENT,
                                buffer, nfs[i].tag, nfs[i].instance_id, nfs[i].service_id, nfs[i].thread_info.core,
                                nfs[i].thread_info.parent, state, rte_atomic16_read(&nfs[i].thread_info.children_cnt),
                                rx, tx, rx_pps, tx_pps, rx_drop, tx_drop, rx_drop_rate, tx_drop_rate,
                                act_out, act_tonf, act_drop, act_next, act_buffer, act_returned,
                                num_wakeups, wakeup_rate);
                } else if (verbosity_level == 2) {
                        fprintf(stats_out, ONVM_STATS_ADV_CONTENT,
                                nfs[i].tag, nfs[i].instance_id, nfs[i].service_id, nfs[i].thread_info.core,
                                rx_pps, tx_pps, rx, tx, act_out, act_tonf, act_drop,
                                nfs[i].thread_info.parent, state, rte_atomic16_read(&nfs[i].thread_info.children_cnt),
                                rx_drop_rate, tx_drop_rate, rx_drop, tx_drop, act_next, act_buffer, act_returned);
                        if (ONVM_NF_SHARE_CORES)
                                fprintf(stats_out, ONVM_STATS_SHARED_CORE_CONTENT, num_wakeups, wakeup_rate);
                        fprintf(stats_out, "\n");
                } else {
                        fprintf(stats_out, ONVM_STATS_REG_CONTENT,
                                nfs[i].tag, nfs[i].instance_id, nfs[i].service_id, nfs[i].thread_info.core,
                                rx_pps, tx_pps, rx_drop, tx_drop, act_out, act_tonf, act_drop);
                }
                /* Only print this information out if we haven't already printed it to the console above */
                if (stats_out != stdout && stats_out != stderr) {
                        ONVM_SNPRINTF(nf_label, 6, "NF %d", i);

                        cJSON_AddItemToObject(onvm_json_nf_stats_obj, nf_label,
                                              onvm_json_nf_stats[i] = cJSON_CreateObject());

                        cJSON_AddStringToObject(onvm_json_nf_stats[i], "Label", nf_label);
                        cJSON_AddNumberToObject(onvm_json_nf_stats[i], "RX", rx_pps);
                        cJSON_AddNumberToObject(onvm_json_nf_stats[i], "TX", tx_pps);
                        cJSON_AddNumberToObject(onvm_json_nf_stats[i], "TX_Drop_Rate", tx_drop_rate);
                        cJSON_AddNumberToObject(onvm_json_nf_stats[i], "RX_Drop_Rate", rx_drop_rate);
                        cJSON_AddNumberToObject(onvm_json_nf_stats[i], "service_id", (int16_t)nfs[i].service_id);
                        cJSON_AddNumberToObject(onvm_json_nf_stats[i], "instance_id",
                                                (int16_t)nfs[i].instance_id);
                        cJSON_AddNumberToObject(onvm_json_nf_stats[i], "core", (int16_t)nfs[i].thread_info.core);

                        free(nf_label);
                        nf_label = NULL;
                }

                nf_rx_last[i] = nfs[i].stats.rx;
                nf_tx_last[i] = nfs[i].stats.tx;
                nf_rx_drop_last[i] = rx_drop;
                nf_tx_drop_last[i] = tx_drop;
        }

        if (verbosity_level == ONVM_RAW_STATS_DUMP)
                return;

        if (print_total_stats) {
                fprintf(stats_out, "\nService id totals\n");
                fprintf(stats_out, "-----------------\n");
                for (i = 0; i < MAX_SERVICES; i++) {
                        uint16_t nfs_for_service = nf_per_service_count[i];
                        const char *nf_count = nfs_for_service == 1 ? "NF " : "NFs";
                        if (nfs_for_service == 0)
                                continue;
                        if (verbosity_level == 2) {
                                fprintf(stats_out, ONVM_STATS_ADV_TOTALS,
                                    i, nfs_for_service, nf_count, rx_pps_for_service[i], tx_pps_for_service[i],
                                    rx_for_service[i], tx_for_service[i], act_out_for_service[i],
                                    act_tonf_for_service[i], act_drop_for_service[i], rx_drop_rate_for_service[i],
                                    tx_drop_rate_for_service[i], rx_drop_for_service[i], tx_drop_for_service[i],
                                    act_next_for_service[i], act_buffer_for_service[i], act_returned_for_service[i]);
                        } else {
                                fprintf(stats_out, ONVM_STATS_REG_TOTALS,
                                        i, nfs_for_service, nf_count, rx_pps_for_service[i], tx_pps_for_service[i],
                                        rx_drop_for_service[i], tx_drop_for_service[i], act_out_for_service[i],
                                        act_tonf_for_service[i], act_drop_for_service[i]);
                        }
                }
        }

        if (ONVM_NF_SHARE_CORES) {
                fprintf(stats_out, "\n\nShared core stats\n");
                fprintf(stats_out, "-----------------\n");
                onvm_stats_display_client_wakeup_thread_context(difftime);
        }

}

/***************************Helper functions**********************************/

static void
onvm_stats_clear_terminal(void) {
        const char clr[] = {27, '[', '2', 'J', '\0'};
        const char topLeft[] = {27, '[', '1', ';', '1', 'H', '\0'};

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
                snprintf(addresses[port], sizeof(addresses[port]), "%02x:%02x:%02x:%02x:%02x:%02x\n", mac.addr_bytes[0],
                         mac.addr_bytes[1], mac.addr_bytes[2], mac.addr_bytes[3], mac.addr_bytes[4], mac.addr_bytes[5]);
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
        fflush(json_events_out);
}

static void
onvm_stats_truncate(void) {
        if (stats_out == stdout || stats_out == stderr) {
                return;
        }

        stats_out = freopen(NULL, ONVM_STATS_FOPEN_ARGS, stats_out);
        json_stats_out = freopen(NULL, ONVM_STATS_FOPEN_ARGS, json_stats_out);
        json_events_out = freopen(NULL, ONVM_STATS_FOPEN_ARGS, json_events_out);

        /* Ensure we're able to open all the files we need */
        if (stats_out == NULL) {
                rte_exit(-1, "Error truncating stats files\n");
        }
}

static void
onvm_json_reset_objects(void) {
        time_t current_time;

        if (onvm_json_root) {
                cJSON_Delete(onvm_json_root);
                onvm_json_root = NULL;
        }

        onvm_json_root = cJSON_CreateObject();

        current_time = time(0);

        cJSON_AddStringToObject(onvm_json_root, ONVM_JSON_TIMESTAMP_KEY, ctime(&current_time));
        cJSON_AddItemToObject(onvm_json_root, ONVM_JSON_PORT_STATS_KEY,
                              onvm_json_port_stats_obj = cJSON_CreateObject());
        cJSON_AddItemToObject(onvm_json_root, ONVM_JSON_NF_STATS_KEY, onvm_json_nf_stats_obj = cJSON_CreateObject());
}
