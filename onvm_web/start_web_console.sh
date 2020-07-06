#!/bin/bash

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

# check if docker is installed
if [$(command -v docker) == ""]
then
  echo "[ERROR] Docker is not installed, please install docker with"
  echo "sudo apt install docker.io"
  exit 1
fi

function usage {
    echo "$0 [-p WEB-PORT-NUMBER]"
    exit 1
}

web_port=8080

while getopts "p:" opt; do
    case $opt in
        p) web_port="$OPTARG";;
        \?) echo "Unknown option -$OPTARG" && usage
            ;;
    esac
done

# Get localhost IP address
# ifconfig | grep inet | grep -v inet6 | grep -v 127 | cut -d '' -f2 | awk '{print $2}'
# export HOSTIP=$!

# Start ONVM web stats console at http://localhost:<port num>
echo -n "Starting openNetVM Web Stats Console at http://localhost:"
echo "$web_port"

is_web_port_in_use=$(sudo netstat -tulpn | grep LISTEN | grep ":$web_port")
if [[ "$is_web_port_in_use" != "" ]]
then
  echo "[ERROR] Web port $web_port is in use"
  echo "$is_web_port_in_use"
  echo "[ERROR] Web stats failed to start"
  exit 1
fi

is_data_port_in_use=$(sudo netstat -tulpn | grep LISTEN | grep ":8000")
if [[ "$is_data_port_in_use" != "" ]]
then
  echo "[ERROR] Port 8000 is in use"
  echo "$is_data_port_in_use"
  echo "[ERROR] Web stats failed to start"
  exit 1
fi

# start Grafana server at http://localhost:3000
echo "Starting Grafana server at http://localhost:3000"
is_grafana_port_in_use=$(sudo netstat -tulpn | grep LISTEN | grep ":3000")
if [[ "$is_grafana_port_in_use" != "" ]]
then
  echo "[ERROR] Grafana port 3000 is in use"
  echo "$is_grafana_port_in_use"
  echo "[ERROR] Grafana server failed to start"
  exit 1
fi

# check if Grafana docker image has been build
is_grafana_build=$(sudo docker images | grep modified_grafana)
if [[ "$is_grafana_build" == "" ]]
then
  sudo docker build -t grafana/modified_grafana ../
fi

# start prometheus server at http://localhost:9090
echo "Starting Prometheus server at http://localhost:9090"
is_prometheus_port_in_use=$(sudo netstat -tulpn | grep LISTEN | grep ":9090")
if [[ "$is_prometheus_port_in_use" != "" ]]
then
  echo "[ERROR] Prometheus port 9090 is in use"
  echo "$is_prometheus_port_in_use"
  echo "[ERROR] Prometheus server failed to start"
  exit 1
fi

# start node exporter server at http://localhost:9100
echo "Starting node exporter server at http://localhost:9100"
is_node_exporter_port_in_use=$(sudo netstat -tulpn | grep LISTEN | grep ":9090")
if [[ "$is_node_exporter_port_in_use" != "" ]]
then
  echo "[ERROR] Node exporter port 9100 is in use"
  echo "$is_prometheus_port_in_use"
  echo "[ERROR] Node exporter server failed to start"
  exit 1
fi

is_grafana_started=$(sudo docker ps -a | grep grafana)
if [[ "$is_grafana_started" == "" ]]
then
  nohup sudo docker run -p 3000:3000 --name grafana grafana/modified_grafana
else
  nohup sudo docker start grafana
fi

is_prometheus_started=$(sudo docker ps -a | grep prometheus)
if [[ "$is_prometheus_started" == "" ]]
then
  nohup sudo docker run -p 9090:9090 --name prometheus -v "$ONVM_HOME"/onvm_web/Prometheus.yml:/etc/prometheus/prometheus.yml prom/prometheus
else
  nohup sudo docker start prometheus
fi

cd "$ONVM_HOME"/onvm_web/node_exporter || usage
sudo chmod a+x node_exporter
sudo nohup ./node_exporter &
export NODE_PID=$!

cd "$ONVM_HOME"/onvm_web || usage
nohup python cors_server.py 8000 &
export ONVM_WEB_PID=$!

cd "$ONVM_HOME"/onvm_web/web-build || usage
nohup python -m SimpleHTTPServer "$web_port" &
export ONVM_WEB_PID2=$!
