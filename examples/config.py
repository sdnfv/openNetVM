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
from signal import signal, SIGINT, SIGKILL
from subprocess import Popen
from datetime import datetime

def handler(signal_received, frame):
    """Handles cleanup on user shutdown"""
    # pylint: disable=unused-argument
    for lf in log_files:
        lf.close()
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

def on_failure():
    """Handles shutdown on error"""
    for lf in log_files:
        lf.close()
    script_pid = os.getpid()
    pid_list = os.popen("ps -ef | awk '{if ($3 == " + str(script_pid) + ") print $2 " " $3}'")
    pid_list = pid_list.read().split("\n")[:-2]
    print(pid_list)
    for i in pid_list:
        i = i.replace(str(script_pid), "")
        temp = os.popen("ps -ef | awk '{if($3 == " + str(i) + ") print $2 " " $3}'")
        temp = temp.read().replace(str(i), "").replace("\n", "")
        _pid_for_nf = os.popen("ps -ef | awk '{if($3 == " + temp + ") print $2 " " $3}'")
        _pid_for_nf = _pid_for_nf.read().replace(temp, "").replace("\n", "")
        os.kill(int(_pid_for_nf), SIGKILL)
        print(_pid_for_nf)
    # for n in procs_list:
    #     try:
    #         os.system("sudo pkill -P" + n.pid)
    #     except OSError:
    #         pass
    print("Error occurred. Exiting...")
    sys.exit(1)

def on_timeout():
    """Handles shutdown on error"""
    for lf in log_files:
        lf.close()
    script_pid = os.getpid()
    pid_list = os.popen("ps -ef | awk '{if ($3 == " + str(script_pid) + ") print $2 " " $3}'")
    pid_list = pid_list.read().split("\n")[:-2]
    for i in pid_list:
        i = i.replace(str(script_pid), "")
        temp = os.popen("ps -ef | awk '{if($3 == " + str(i) + ") print $2 " " $3}'")
        temp = temp.read().replace(str(i), "").replace("\n", "")
        _pid_for_nf = os.popen("ps -ef | awk '{if($3 == " + temp + ") print $2 " " $3}'")
        _pid_for_nf = _pid_for_nf.read().replace(temp, "").replace("\n", "")
        os.kill(int(_pid_for_nf), SIGKILL)
        print(_pid_for_nf)
    print("Exiting...")
    sys.exit(0)

def running_services():
    """Checks running NFs"""
    if timeout != 0:
        start = time.time()
        time.clock()
        elapsed = 0
        while elapsed < timeout:
            elapsed = time.time() - start
            for pl in procs_list:
                ret_code = pl.poll()
                if ret_code is not None:
                    on_failure()
                    break
            time.sleep(.1)
        on_timeout()
    else:
        while 1:
            for pl in procs_list:
                ret_code = pl.poll()
                if ret_code is not None:
                    on_failure()
                    break
            time.sleep(.1)

procs_list = []
nf_list = []
cmds_list = []
log_files = []
timeout = 0
if __name__ == '__main__':
    signal(SIGINT, handler)
    if os.path.basename(os.getcwd()) != "examples":
        print("Error: Run script from within /examples folder")
        sys.exit(1)

    config_file = get_config()

    with open(config_file) as f:
        try:
            data = json.load(f)
        except:
            print("Cannot load config file. Check JSON syntax")
            sys.exit(1)

    is_dir = 0
    for k, v in data.items():
        if k == "objects":
            for item in v:
                if "directory" in item:
                    if is_dir == 0:
                        log_dir = item["directory"]
                        if os.path.isdir(log_dir) is False:
                            try:
                                os.mkdir(log_dir)
                                print("Creating directory %s" % (log_dir))
                                is_dir = 1
                            except OSError:
                                print("Creation of directory %s failed" % (log_dir))
                                on_failure()
                        else:
                            print("Outputting log files to %s" % (log_dir))
                            is_dir = 1
                if "TTL" in item:
                    timeout = item["TTL"]
        else:
            for item in v:
                nf_list.append(k)
                cmds_list.append("./go.sh " + item['parameters'])

    if is_dir == 0:
        time_obj = datetime.now().time()
        log_dir = time_obj.strftime("%H:%M:%S")
        os.mkdir(log_dir)
        print("Creating directory %s" % (log_dir))
        is_dir = 1

    for cmd, nf in zip(cmds_list, nf_list):
        service_name = nf
        instance_id = cmd.split()
        log_file = "log-" + nf + "-" + instance_id[1] + ".txt"
        log_files.append(open(os.path.join(log_dir, log_file), 'w'))

    i = 0
    cwd = os.getcwd()
    for cmd, nf in zip(cmds_list, nf_list):
        try:
            os.chdir(nf)
        except OSError:
            print("Error: Unable to access NF directory %s." \
                " Check syntax in your configuration file" % (nf))
            sys.exit(1)
        try:
            p = Popen(shlex.split(cmd), stdout=(log_files[i]), stderr=log_files[i], \
                universal_newlines=True)
            procs_list.append(p)
            print("Starting %s %s" % (nf, cmd), flush=True)
            i += 1
        except OSError:
            pass
        os.chdir(cwd)

    running_services()