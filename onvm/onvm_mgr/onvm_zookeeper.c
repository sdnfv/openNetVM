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
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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

                              onvm_zookeeper.c

    File that deals interfaces with Zookeeper for distributed config


******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <rte_log.h>
#include <zookeeper/zookeeper.h>

#include "onvm_common.h"
#include "onvm_init.h"
#include "onvm_nf.h"
#include "onvm_zookeeper.h"
#include "onvm_zk_common.h"
#include "onvm_zk_watch.h"
#include "zoo_queue.h"

#define EXPIRATION_CACHE_LEN 10 
#define MAC_STR_LEN 18

// Handle to our zookeeper connection
static zhandle_t *zh = NULL;
static const clientid_t *myid = NULL;
static char *nf_stat_paths[MAX_NFS];

// Cache of remote MAC addresses
struct remote_service_result {
        char mac_address[MAC_STR_LEN];
        struct ether_addr mac_address_struct;
        time_t expiration;
};
static struct remote_service_result service_lookup_cache[MAX_SERVICES];

//static time_t get_service_last_modified(uint16_t service_id);
//static uint16_t can_scale_locally(uint16_t service_id);
//static int64_t can_scale_remotely(uint16_t service_id);
//static int enqueue_remote_scale_msg(int64_t manager_id, uint16_t service_id);

static inline int update_service_last_modified(uint16_t service_id);
static inline int mac_string_to_struct(const char *data, struct ether_addr *addr);
static inline void free_String_vector(struct String_vector *v);

int
onvm_zk_connect(int mode) {

        if (zh) return 0;
        zh = zookeeper_init(ZK_SERVER, onvm_zk_watcher, 3000, myid, NULL, 0);
        if (!zh) return -1;

        if (mode != ZK_CONNECT_BLOCKING)
                return 0;

        // Wait for the connection to be established
        sleep(1);
        while (zoo_state(zh) != ZOO_CONNECTED_STATE) {
                sleep(3);
        }
        myid = zoo_client_id(zh);

        return 0;
}

int64_t
onvm_zk_client_id(void) {
        if (!zh || !myid) return 0;
        return myid->client_id;
}

int
onvm_zk_init(const char *port_addr) {
        int64_t zk_id;
        size_t addr_len;
        char path_buf[128];
        int ret;

        if (!zh) return ZINVALIDSTATE;
        zk_id = onvm_zk_client_id();

        // Ensure the parent node for our manager node exists
        // Create it if it does not
        ret = onvm_zk_create_if_not_exists(zh, MGR_NODE_BASE, "", 0, 0, NULL, 0);
        if (ret != ZOK) {
                return ret;
        }

        // Create the base node for NF stats
        ret = onvm_zk_create_if_not_exists(zh, NF_NODE_BASE, "", 0, 0, NULL, 0);
        if (ret != ZOK) {
                return ret;
        }

        // Create an ephemeral node for this instance
        // We won't have to clean this up, since it'll go away when the manager exits
        sprintf(path_buf, MGR_NODE_FMT, zk_id);
        addr_len = strlen(port_addr);
        ret = zoo_create(zh, path_buf, port_addr, addr_len, &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL, NULL, 0);

        // Ensure the global queue base node exists
        ret = onvm_zk_create_if_not_exists(zh, SCALE_QUEUE_BASE, "", 0, 0, NULL, 0);
        if (ret != ZOK) {
                return ret;
        }

        // Set up the remote lookup cache
        memset(service_lookup_cache, 0, MAX_SERVICES * sizeof(struct remote_service_result));

        return ret;
}

int
onvm_zk_nf_start(uint16_t service_id, uint16_t service_count, uint16_t instance_id) {
        int64_t zk_id;
        char path_buf[128];
        char data_buf[32];
        char res_path_buf[32];
        size_t data_len;
        int ret;

        if (!zh) return ZINVALIDSTATE;
        zk_id = onvm_zk_client_id();

        // Ensure the parent node for our manager node exists
        // Create it if it does not
        ret = onvm_zk_create_if_not_exists(zh, SERVICE_NODE_BASE, "", 0, 0, NULL, 0);
        if (ret != ZOK) {
                return ret;
        }

        // Create a node for this service, if needed
        ret = update_service_last_modified(service_id);
        if (ret != ZOK) {
                return ret;
        }

        // Create nodes for this service + NF's stats, base node first
        sprintf(path_buf, NF_SERVICE_BASE, service_id);
        ret = onvm_zk_create_if_not_exists(zh, path_buf, "", 0, 0, NULL, 0);
        if (ret != ZOK) {
                return ret;
        }

        // And now for this NF's stats
        sprintf(path_buf, NF_INSTANCE_FMT, service_id);
        sprintf(data_buf, NF_STAT_FMT, 0.0); // the ring starts at 0% used
        ret = onvm_zk_create_if_not_exists(zh, path_buf, data_buf, strlen(data_buf), ZOO_SEQUENCE|ZOO_EPHEMERAL, res_path_buf, sizeof(res_path_buf) - 1);
        if (ret != ZOK) {
                return ret;
        }
        nf_stat_paths[instance_id] = malloc(strlen(res_path_buf));
        if (!nf_stat_paths[instance_id]) {
                return ZINVALIDSTATE;
        }
        strncpy(nf_stat_paths[instance_id], res_path_buf, strlen(res_path_buf));
        printf("Created stat node (%s): %s\n", zk_status_to_string(ret), nf_stat_paths[instance_id]);

        // Create a node for this (service + manager) pair, if needed, else updadte the value
        sprintf(path_buf, SERVICE_INSTANCE_FMT, service_id, zk_id);
        sprintf(data_buf, SERVICE_COUNT_FMT, service_count);
        data_len = strlen(data_buf);
        ret = onvm_zk_create_or_update(zh, path_buf, data_buf, data_len, ZOO_EPHEMERAL);

        return ret;
}

int
onvm_zk_nf_stop(uint16_t service_id, uint16_t service_count, uint16_t instance_id) {
        int64_t zk_id;
        char path_buf[128];
        char data_buf[32];
        size_t data_len;
        int ret;

        if (!zh) return ZINVALIDSTATE;
        zk_id = onvm_zk_client_id();

        sprintf(path_buf, SERVICE_INSTANCE_FMT, service_id, zk_id);

        // If there are no services running locally, we can delete the node
        if (service_count == 0) {
                ret = zoo_delete(zh, path_buf, -1);
        } else {
                sprintf(data_buf, SERVICE_COUNT_FMT, service_count);
                data_len = strlen(data_buf);
                ret = zoo_set(zh, path_buf, data_buf, data_len, -1);
        }

        // Update this service last changed time
        ret = update_service_last_modified(service_id);

        // Delete the NF's stats node
        ret = zoo_delete(zh, nf_stat_paths[instance_id], -1);
        free(nf_stat_paths[instance_id]);

        return ret;
}

void
onvm_zk_disconnect(void) {
        if (!zh) return;
        zookeeper_close(zh);
        myid = NULL;
}

int64_t
onvm_zk_lookup_service(struct rte_mbuf *pkt, uint16_t service_id, struct ether_addr *dst) {
        char path_buf[128];
        char data_buf[MAC_STR_LEN + 1];
        struct String_vector children;
        struct Stat stat;
        time_t now;
        int ret;
        int data_len;
        int64_t result;
        int chosen_instance_index;

        if (!zh) return 0;

        /* First, check our cache */
        now = time(NULL);
        if (now <= service_lookup_cache[service_id].expiration) {
                dst = &service_lookup_cache[service_id].mac_address_struct;
                return strlen(service_lookup_cache[service_id].mac_address) ? -1 : 0;
        }

        sprintf(path_buf, SERVICE_NODE_FMT, service_id);
        ret = zoo_get_children2(zh, path_buf, 0, &children, &stat);
        if (ret != ZOK || stat.numChildren == 0) {
                return 0;
        }

        // Assume this service is not running locally so just take the first
        chosen_instance_index = pkt->hash.rss % children.count;
        sprintf(path_buf, MGR_NODE_STR_FMT, children.data[chosen_instance_index]);
        data_len = sizeof(data_buf) / sizeof(char);
        ret = zoo_get(zh, path_buf, 0, data_buf, &data_len, &stat);
        if (ret != ZOK || (data_len == 0 && stat.dataLength == 0)) {
                printf("Can't get dest @ %s (%s)\n", path_buf, zk_status_to_string(ret));
                result = 0;
                goto lookup_done;
        }
        data_buf[data_len] = '\0';

        /* Cache the resulting MAC address */
        // TODO should use the flow table, this may vary per-flow
        strncpy(service_lookup_cache[service_id].mac_address, data_buf, MAC_STR_LEN);
        mac_string_to_struct(data_buf, &service_lookup_cache[service_id].mac_address_struct);
        service_lookup_cache[service_id].expiration = now + EXPIRATION_CACHE_LEN;

        dst = &service_lookup_cache[service_id].mac_address_struct;
        result = mac_string_to_struct(data_buf, dst)
                ? stat.ephemeralOwner
                : 0;
lookup_done:
        free_String_vector(&children);
        return result;
}

#if 0
int
onvm_zk_update_nf_stats(uint16_t service_id, uint16_t instance_id, cJSON *stats_json) {
        char *json_string = NULL;
        double rx_use;
        time_t last_update;
        time_t now;
        uint16_t local_instance;
        int64_t remote_instance;
        cJSON *rx_use_json;
        int ret;

        if (!zh || !nf_stat_paths[instance_id]) return ZINVALIDSTATE;

        ret = zoo_exists(zh, nf_stat_paths[instance_id], 0, NULL);
        if (ret != ZOK) goto done;

        /* Build a JSON String of the stat data */
        json_string = cJSON_Print(stats_json);
        if (!json_string) return ZBADARGUMENTS;

        /* Update the state values in ZooKeeper */
        ret = zoo_set(zh, nf_stat_paths[instance_id], json_string, strlen(json_string), -1);
        if (ret != ZOK) goto done;

        /* Make scheduling decisions */
        rx_use_json = cJSON_GetObjectItem(stats_json, "RX Use");
        if (!rx_use_json) {
                RTE_LOG(INFO, APP, "Can't get RX Use from JSON\n");
                goto done;
        }
        rx_use = rx_use_json->valuedouble;
        now = time(NULL);
        last_update = get_service_last_modified(service_id);
        printf("Instance %u RX use: %f\n", instance_id, rx_use);

        /* Only scale if the RX Queue is saturated */
        if (rx_use < SCALE_RX_USE_MAX) goto done;

        /* Do not start or stop an instance within 30 seconds of the last action */
        printf("Now %lu, timeout %lu\n", (unsigned long)now, (unsigned long)(last_update + SCALE_TIMEOUT_SEC));
        if (now < last_update + SCALE_TIMEOUT_SEC) goto done;

        /* We want to start instances on the same machine, where we can */
        local_instance = can_scale_locally(service_id);
        if (local_instance != 0) {
                /* Send scale message to local instance */
                ret = onvm_nf_send_msg(local_instance, MSG_SCALE, MSG_MODE_ASYNCHRONOUS, NULL);
                if (ret != 0) RTE_LOG(INFO, APP, "Unable to tell NF %u to scale: %d\n", local_instance, ret);
                else update_service_last_modified(service_id);
                goto done;
        }

        remote_instance = can_scale_remotely(service_id);
        if (remote_instance != 0) {
                enqueue_remote_scale_msg(remote_instance, service_id);
                goto done;
        }

done:
        if (json_string) free((void*)json_string);
        return ret;
}
#endif

static inline int
update_service_last_modified(uint16_t service_id) {
        char path_buf[128];
        char data_buf[32];
        int len;
        time_t current_time;

        if (!zh) return ZINVALIDSTATE;

        sprintf(path_buf, SERVICE_NODE_FMT, service_id);
        current_time = time(NULL);
        sprintf(data_buf, "%lu", (unsigned long)current_time);
        len = strlen(data_buf);
        printf("Updating last modified for service %u at %s (%d): %s\n", service_id, path_buf, len, data_buf);
        return onvm_zk_create_or_update(zh, path_buf, data_buf, len, 0);
}

#if 0
static time_t
get_service_last_modified(uint16_t service_id) {
        char path_buf[32];
        char data_buf[32];
        struct Stat stat;
        time_t last_time;
        int data_len;
        int ret;

        if (!zh) return 0;

        sprintf(path_buf, SERVICE_NODE_FMT, service_id);
        data_len = sizeof(data_buf) / sizeof(char);
        ret = zoo_get(zh, path_buf, 0, data_buf, &data_len, &stat);
        if (ret != ZOK || data_len <= 0) {
                printf("Can't get last modified for service %s: %s, len: %d\n", path_buf, zk_status_to_string(ret), data_len);
                return 0;
        }

        data_buf[data_len] = '\0';
        printf("Last modified %s: %s (%d)\n", path_buf, data_buf, data_len);
        last_time = (time_t) strtoul(data_buf, NULL, 10);
        return last_time;
}

static uint16_t
can_scale_locally(uint16_t service_id) {
        struct onvm_nf_info *info;
        uint16_t best_instance_id = 0;
        int best_headroom = 0;
        int i;

        for (i = 0; i < MAX_NFS; i++) {
                if (!onvm_nf_is_valid(&nfs[i])) continue;
                info = nfs[i].info;
                if (info->service_id == service_id && info->headroom > best_headroom) {
                        best_headroom = info->headroom;
                        best_instance_id = info->instance_id;
                }
        }
        return best_instance_id;
}

static int
enqueue_remote_scale_msg(int64_t manager_id, uint16_t service_id) {
        char path_buf[32];
        char data_buf[32];
        zkr_queue_t queue;

        if (!zh) return ZINVALIDSTATE;

        sprintf(path_buf, SCALE_QUEUE_FMT, manager_id);
        zkr_queue_init(&queue, zh, path_buf, NULL);

        sprintf(data_buf, SCALE_DATA_FMT, service_id);
        return zkr_queue_offer(&queue, data_buf, strlen(data_buf));
}

static int64_t
can_scale_remotely(uint16_t service_id) {
        struct String_vector children;
        struct Stat stat;
        char path_buf[128];
        char data_buf[128];
        cJSON *stat_data;
        cJSON *headroom_data;
        int64_t best_instance;
        int data_len;
        int best_headroom;
        int headroom;
        int ret;
        int i;

        if (!zh) return 0;

        sprintf(path_buf, NF_SERVICE_BASE, service_id);
        ret = zoo_get_children2(zh, path_buf, 0, &children, &stat);
        if (ret != ZOK || stat.numChildren == 0) return 0;

        sprintf(path_buf, MGR_NODE_STR_FMT, children.data[0]);
        best_instance = 0;
        best_headroom = 0;
        for (i = 0; i < children.count; i++) {
                sprintf(path_buf, NF_STAT_CHILD_FMT, service_id, children.data[i]);
                data_len = sizeof(data_buf) / sizeof(char);
                ret = zoo_get(zh, path_buf, 0, data_buf, &data_len, &stat);
                if (ret != ZOK || data_len == 0) continue;

                stat_data = cJSON_Parse(data_buf);
                headroom_data = cJSON_GetObjectItem(stat_data, "Free Cores");
                if (!headroom_data) continue;
                headroom = headroom_data->valuedouble;
                if (headroom > best_headroom) {
                        best_headroom = headroom;
                        best_instance = stat.ephemeralOwner;
                }
                cJSON_Delete(stat_data);
        }

        free_String_vector(&children);
        return best_instance;
}
#endif

static inline int
mac_string_to_struct(const char *data, struct ether_addr *addr) {
        unsigned int temp[ETHER_ADDR_LEN];
        int bytes_found;
        int i;

        bytes_found = sscanf(data, MAC_ADDR_FMT,
                &temp[0],
                &temp[1],
                &temp[2],
                &temp[3],
                &temp[4],
                &temp[5]);
        if (bytes_found != ETHER_ADDR_LEN) {
                return 0;
        }

        for (i = 0; i < ETHER_ADDR_LEN; i++) {
                addr->addr_bytes[i] = (uint8_t) temp[i];
        }
        return ETHER_ADDR_LEN;
}

static inline void
free_String_vector(struct String_vector *v) {
    if (v->data) {
        int32_t i;
        for (i=0; i<v->count; i++) {
            free(v->data[i]);
        }
        free(v->data);
        v->data = 0;
    }
}
