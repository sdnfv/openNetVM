#! /bin/bash

#                        openNetVM
#                https://sdnfv.github.io
#
# OpenNetVM is distributed under the following BSD LICENSE:
#
# Copyright(c)
#       2015-2024 George Washington University
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
# A script to install required linux packages and perform initial developer 
# environment setup actions.

packages=("build-essential" \
          "python3" \
          "python3-pip" \
          "python3-setuptools" \
          "python3-wheel" \
          "python3-venv" \
          "ninja-build" \
          "pkg-config" \
          "libnuma-dev" \
          "libpcap-dev" \
          "libsystemd-dev")
install_packages=true

pypackages=("meson" \
            "pyelftools")
pyenv="env"

# Check the passed arguments, and set the appropriate flags if a
# particular argument is detected
for arg in "$@"
do
    if [[ $arg == "--noinstall" ]]; then
        install_packages=false
        break
    fi
done

# Check to make sure this script is running in the correct working
# directory.
# Ensure we're working relative to the onvm root directory
if [ "$(basename "$(pwd)")" != "openNetVM" ]; then
    echo "Please run the installation script from the parent openNetVM directory"
fi


# (1)
# Install required packages for development
required=$(IFS=' '; echo "${packages[*]}")

echo "- Installing required packages"
if [ "$install_packages" = true ]; then
    echo "  - installing: $required"
    sudo apt-get update
    sudo apt-get install $required -y
else
    echo "  - skipping due to --noinstall flag"
fi


# (2)
# Initialize the Git submodules (dpdk & pkt_gen)
echo "- Initializing Git submodules"

git submodule update --init


# (3)
# Create the Python environment (this will be used for compiling onvm)
pyrequired=$(IFS=' '; echo "${pypackages[*]}")

echo "- Setup Python environment"
echo "  - installing $pyrequired"

python3 -m venv $pyenv
source env/bin/activate
pip3 install $pyrequired


# (4)
# Build the dpdk-kmods uio kernel module
echo "- Building the dpdk-kmods uio kernel module"
cd subprojects/dpdk-kmods/linux/igb_uio
make
cd -

# (5)
# Install dpdk
echo "- Installing dpdk"
cd subprojects/dpdk
meson build
ninja -C build
ninja -C build install
sudo ldconfig
