#!usr/bin/env python3

import socket
import json
import time
from urllib import error
from prometheus_client import CollectorRegistry, Gauge, push_to_gateway

def create_collector(name, documentation):
    registry = CollectorRegistry()
    g = Gauge(name, documentation, registry=registry)
    return registry, g

def push_data(url, job_name, registry):
    try:
        push_to_gateway(url, job_name, registry)
        return 1
    except error.URLError:
        return -1

if __name__ == "__main__":
    host_name = socket.gethostname()
    host_ip = socket.gethostbyname(host_name)
    gateway_url = host_ip + ":9091"

    registry_nf, gauge_data = create_collector('onvm_nf_stats', 'collected nf stats')

    is_connection_failed = 1
    while is_connection_failed != -1:
        try:
            with open("./onvm_json_stats.json", 'r') as data_f:
                data = json.load(data_f)
            with open("./onvm_json_events.json", 'r') as events_f:
                event = json.load(events_f)
        except:
            continue
        last_update_time = time.mktime(time.strptime(data['last_updated'], "%a %b %d %H:%M:%S %Y\n"))
        current_time = time.time()
        if current_time - last_update_time > 5:
            pass
        else:
            try:
                nf_list = []
                for nfs in event:
                    if nfs['message'] == "NF Starting":
                        nf_name = nfs['source']['type'] + nfs['source']['instance_id']
                        if nf_name not in nf_list:
                            nf_list.append(nf_name)
                nf_stats = data['onvm_nf_stats']
                counter = 0
                for keys, values in nf_stats.items():
                    gauge_data.set(values['RX'])
                    nf_name = nf_list[counter]
                    is_connection_failed = push_data(gateway_url, nf_name, registry_nf)
                    counter += 1
            except KeyError:
                continue
        time.sleep(1)