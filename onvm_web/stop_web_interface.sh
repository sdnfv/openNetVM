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

function usage {
    echo "$0 -r remove build docker images"
    echo -e "\tRemove grafana and/or prometheus docker image"
    exit 1
}

remove=0

while getopts "r:" opt; do
    case $opt in
        r) remove=$OPTARG;;
        \?) echo "Unknown option -$OPTARG" && usage
            ;;
    esac
done

is_grafana_running=$(sudo docker container ls | grep grafana)
if [[ is_grafana_running != "" ]]
then
  sudo docker stop grafana
fi

is_prometheus_running=$(sudo docker container ls | grep prometheus)
if [[ is_prometheus_running != "" ]]
then
  sudo docker stop prometheus
fi

# remove grafana image
is_grafana_image_build=$(sudo docker images | grep grafana/modified_grafana)
if [[ $remove =~ "grafana" && is_grafana_image_build != "" ]]
then
  echo "removing grafana image"
  sudo docker rm grafana
  sudo docker rmi grafana/modified_grafana
fi

# remove prometheus image
is_prometheus_image_build=$(sudo docker images | grep prom/prometheus)
if [[ $remove =~ "prometheus" && is_prometheus_image_build != "" ]]
then
  echo "removing prometheus image"
  sudo docker rm prometheus
  sudo docker rmi prom/prometheus
fi

onvm_web_pid=$(ps -ef | grep cors | grep -v "grep" | awk '{print $2}')
onvm_web_pid2=$(ps -ef | grep Simple | grep -v "grep" | awk '{print $2}')
node_pid=$(ps -ef | grep node | grep -v "grep" | awk '{print $2}')

if [[ onvm_web_pid != "" ]]
then
  kill ${onvm_web_pid}
fi

if [[ onvm_web_pid2 != "" ]]
then
  kill ${onvm_web_pid2}
fi

if [[ node_pid != "" ]]
then
  kill ${node_pid}
fi