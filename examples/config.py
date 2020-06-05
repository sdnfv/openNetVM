#!/usr/bin/env python3

import sys
import json
import os
import shlex
import time
from signal import signal, SIGINT
from subprocess import Popen, PIPE

# cleanup on user shutdown
def handler(signal_received, frame):
    print("\nExiting...")
    for proc in procs_list:
        try:
            proc.kill()
        except OSError:
            pass
    sys.exit(0)

# get path to config file words
def get_config():
    if len(sys.argv) < 2:
        print("Error: Specify config file")
        sys.exit(0)

    file_name = sys.argv[1]
    if os.path.exists(file_name):
        path_to_file = os.path.join(os.path.abspath(os.getcwd()), file_name)
        print(path_to_file)
        return path_to_file
    print("Error: No config file found")
    sys.exit(0)

# strip parameters
def remove_prefix(text, prefix):
    if text.startswith(prefix):
        return text[len(prefix):]
    return text

# handle shutdown of all NFs on error
def on_failure(error_output):
    print(error_output)
    for proc in procs_list:
        try:
            proc.kill()
        except OSError:
            pass
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
    # user shutdown
    signal(SIGINT, handler)

    # get path to config file
    path = get_config()

    # open file
    with open(path) as f:
        data = json.load(f)
    # parse config
    for k, v in data.items():
        for item in v:
            nf_list.append(k)
            PARAM = str(item).strip("'{[]}'")
            PARAM = remove_prefix(PARAM, "u'parameters': u'")
            cmds_list.append("./go.sh " + PARAM)

    # start NFs
    cwd = os.getcwd()
    for cmd, nf in zip(cmds_list, nf_list):
        os.chdir(nf)
        try:
            p = Popen(shlex.split(cmd), stdout=PIPE, stderr=PIPE)
            procs_list.append(p)
        except OSError:
            pass
        os.chdir(cwd)
    
    # check NFs are running
    while 1:
        for p in procs_list:
            ret_code = p.poll()
            if ret_code is not None:
                out, err = p.communicate()
                on_failure(out + err)
                break
            if ret_code is not None:
                time.sleep(.1)
                continue
