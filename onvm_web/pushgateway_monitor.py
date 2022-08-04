#!usr/bin/env python3

#                        openNetVM
#                https://sdnfv.github.io
#
# OpenNetVM is distributed under the following BSD LICENSE:
#
# Copyright(c)
#       2015-2017 George Washington University
#       2015-2017 University of California Riverside
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in
#   the documentation and/or other materials provided with the
#   distribution.
# * The name of the author may not be used to endorse or promote
#   products derived from this software without specific prior
#   written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

"""This script willpush data to the prometheus pushgateway data collector"""

import socket
import json
import time
from urllib import error
from prometheus_client import CollectorRegistry, Gauge, push_to_gateway


def create_collector(name, documentation):
    """Create a new collector with the name and documentation"""
    registry = CollectorRegistry()
    g = Gauge(name, documentation, registry=registry)
    return registry, g


def push_data(url, job_name, registry):
    """Push data to the designated registor"""
    try:
        push_to_gateway(url, job_name, registry)
        return 1
    except error.URLError:
        return -1


if __name__ == "__main__":
    host_name = socket.gethostname()
    host_ip = socket.gethostbyname(host_name)
    gateway_url = host_ip + ":9091"

    registry_nf, gauge_data = create_collector(
        'onvm_nf_stats', 'collected nf stats')

    # keep trying to push data unless the connection failed
    is_connection_failed = 1
    while is_connection_failed != -1:
        try:
            with open("./onvm_json_stats.json", 'r') as data_f:
                data = json.load(data_f)
            with open("./onvm_json_events.json", 'r') as events_f:
                event = json.load(events_f)
        except:
            continue
        # determine if the data is in a valid time interval
        last_update_time = time.mktime(time.strptime(
            data['last_updated'], "%a %b %d %H:%M:%S %Y\n"))
        current_time = time.time()
        # for now set the time interval to be 5 seconds.
        if current_time - last_update_time > 5:
            pass
        else:
            try:
                nf_list = []
                for nfs in event:
                    if nfs['message'] == "NF Starting":
                        # nf_name constitute the name and the instance id of this nf.
                        nf_name = str(nfs['source']['type']) + \
                            str(nfs['source']['instance_id'])
                        if nf_name not in nf_list:
                            nf_list.append(nf_name)
                nf_stats = data['onvm_nf_stats']
                counter = 0
                for keys, values in nf_stats.items():
                    gauge_data.set(values['RX'])
                    nf_name = nf_list[counter] + "_rx"
                    is_connection_failed = push_data(
                        gateway_url, nf_name, registry_nf)
                    gauge_data.set(values['TX'])
                    nf_name = nf_list[counter] + "_tx"
                    is_connection_failed = push_data(
                        gateway_url, nf_name, registry_nf)
                    counter += 1
            except KeyError:
                continue
        # sleep 1 seconds to wait for the data get processed
        time.sleep(1)
