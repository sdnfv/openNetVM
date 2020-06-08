#!/usr/bin/env python3

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

"""This script allows a user to launch a chain of NFs from a JSON
config file passed in as an argument when running the script"""

import sys
import json
import os
import shlex
import time
from signal import signal, SIGINT
from subprocess import Popen, PIPE

def handler(signal_received, frame):
    """Handles cleanup on user shutdown"""
    # pylint: disable=unused-argument
    print("\nExiting...")
    sys.exit(0)

def get_config():
    """Gets path to JSON config file"""
    if len(sys.argv) < 2:
        print("Error: Specify config file")
        sys.exit(0)

    file_name = sys.argv[1]
    if os.path.exists(file_name):
        return file_name
    print("Error: No config file found")
    sys.exit(0)

def on_failure(error_output):
    """Handles shutdown on error"""
    print(error_output)
    for n in nf_list:
        try:
            os.system("sudo pkill " + n)
        except OSError:
            pass
    print("Error occurred. Exiting...")
    sys.exit(1)

procs_list = []
nf_list = []
cmds_list = []
if __name__ == '__main__':
    if os.path.basename(os.getcwd()) != "examples":
        print("Error: Run script from within /examples folder")
        sys.exit(1)

    signal(SIGINT, handler)

    config_file = get_config()

    with open(config_file) as f:
        data = json.load(f)
    for k, v in data.items():
        for item in v:
            nf_list.append(k)
            cmds_list.append("./go.sh " + item['parameters'])

    cwd = os.getcwd()
    for cmd, nf in zip(cmds_list, nf_list):
        try:
            os.chdir(nf)
        except OSError:
            print("Error: Unable to access NF directory %s." \
                " Check syntax in your configuration file" % (nf))
            sys.exit(1)
        try:
            p = Popen(shlex.split(cmd), stdout=PIPE, stderr=PIPE)
            procs_list.append(p)
        except OSError:
            pass
        os.chdir(cwd)

    while 1:
        for p in procs_list:
            ret_code = p.poll()
            if ret_code is not None:
                out, err = p.communicate()
                on_failure(out)
                break
            time.sleep(.1)
            continue
