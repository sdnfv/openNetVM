#                        openNetVM
#                https://sdnfv.github.io
#
# OpenNetVM is distributed under the following BSD LICENSE:
#
# Copyright(c)
#       2015-2018 George Washington University
#       2015-2018 University of California Riverside
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

FROM ubuntu:20.04
LABEL maintainer="Kevin Deems kevin8deems@gmail.com"
LABEL version="OpenNetVM v20.10"
LABEL vendor="SDNFV @ UCR and GW"
LABEL github="github.com/sdnfv/openNetVM"

ENV ONVM_HOME=/openNetVM
ENV RTE_TARGET=x86_64-native-linuxapp-gcc
ENV RTE_SDK=$ONVM_HOME/dpdk
ENV ONVM_NUM_HUGEPAGES=1024

WORKDIR $ONVM_HOME

# Ubuntu 20.04 image hangs unless timezone is set
RUN DEBIAN_FRONTEND=noninteractive \
    ln -fs /usr/share/zoneinfo/America/New_York /etc/localtime

RUN apt-get update -y && apt-get upgrade -y && \
    apt-get install -y \
    build-essential \
    sudo \
    gdb \
    python \
    libnuma-dev \
    vim \
    less \
    git \
    net-tools \
    bc