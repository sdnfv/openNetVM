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

                                 onvm_zk_common.c

                        Simple Zookeeper helper functions


******************************************************************************/

#include "onvm_zk_common.h"

inline int
onvm_zk_create_if_not_exists(zhandle_t *zh, const char* path, const char *data, int data_len, int flags, char *path_buffer, int path_buffer_len) {
        int ret;

        ret = zoo_exists(zh, path, 0, NULL);
        if (ret == ZNONODE) {
                ret = zoo_create(zh, path, data, data_len, &ZOO_OPEN_ACL_UNSAFE, flags, path_buffer, path_buffer_len);
                if (ret != ZOK) {
                        // We were unable to create the node
                        return ret;
                }
        } else if (ret != ZOK) {
                // Some error getting the status
                return ret;
        }
        return ZOK;
}

inline int
onvm_zk_create_or_update(zhandle_t *zh, const char* path, const char *data, int data_len, int flags) {
        int ret;

        ret = zoo_exists(zh, path, 0, NULL);
        if (ret == ZNONODE) {
                ret = zoo_create(zh, path, data, data_len, &ZOO_OPEN_ACL_UNSAFE, flags, NULL, 0);
        } else if (ret == ZOK) {
                ret = zoo_set(zh, path, data, data_len, -1);
        }
        return ret;
}

const char *
zk_state_to_string(int state) {
        if (state == 0)
                return "CLOSED_STATE";
        if (state == ZOO_CONNECTING_STATE)
                return "CONNECTING_STATE";
        if (state == ZOO_ASSOCIATING_STATE)
                return "ASSOCIATING_STATE";
        if (state == ZOO_CONNECTED_STATE)
                return "CONNECTED_STATE";
        if (state == ZOO_EXPIRED_SESSION_STATE)
                return "EXPIRED_SESSION_STATE";
        if (state == ZOO_AUTH_FAILED_STATE)
                return "AUTH_FAILED_STATE";

        return "INVALID_STATE";
}

const char *
zk_event_type_to_string(int type) {
        if (type == ZOO_CREATED_EVENT)
                return "CREATED_EVENT";
        if (type == ZOO_DELETED_EVENT)
                return "DELETED_EVENT";
        if (type  == ZOO_CHANGED_EVENT)
                return "CHANGED_EVENT";
        if (type == ZOO_CHILD_EVENT)
                return "CHILD_EVENT";
        if (type == ZOO_SESSION_EVENT)
                return "SESSION_EVENT";
        if (type  == ZOO_NOTWATCHING_EVENT)
                return "NOTWATCHING_EVENT";

        return "UNKNOWN_EVENT_TYPE";
}

const char *
zk_status_to_string(int status) {
        if (status == ZOK)
                return "OK";
        if (status == ZNONODE)
                return "NO_NODE";
        if (status == ZNODEEXISTS)
                return "NODE_EXISTS";
        if (status == ZNOAUTH)
                return "NO_AUTH";
        if (status == ZBADVERSION)
                return "BAD_VERSION";
        if (status == ZNOTEMPTY)
                return "NOT_EMPTY";
        if (status == ZBADARGUMENTS)
                return "BAD_ARGUMENTS";
        if (status == ZINVALIDSTATE)
                return "INVALID_STATE";
        if (status == ZMARSHALLINGERROR)
                return "MARSHALLING_ERROR";
        if (status == ZNOCHILDRENFOREPHEMERALS)
                return "NO_CHILDREN_FOR_EPHEMERALS";

        return "UNKNOWN_STATUS";
}


