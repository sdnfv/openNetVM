import sys
import json
import os
import time
from signal import signal, SIGINT
from multiprocessing import Process
from subprocess import Popen, PIPE

# cleanup
def handler(signal_received, frame):
    print("Exiting...")
    for p in procs_list:
        p.kill()
    sys.exit(0)

# check that all processes started 
def check_process():
    while True:
        for j in jobs:
            if not j.is_alive():
                sys.exit(1)

# get path to config file 
def get_config():
    # no config file passed as arg
    if (len(sys.argv) < 2):
        print ("Error: Specify config file")
        sys.exit(0)

    # get path of config file 
    fn = sys.argv[1]
    if os.path.exists(fn):
        path = os.path.join(os.path.abspath(os.getcwd()), os.path.basename(fn))
        return path
    else:
        print ("Error: No config file found")
        sys.exit(0)

# strip parameters
def remove_prefix(text, prefix):
    if text.startswith(prefix):
        return text[len(prefix):]
    return text

# handle shutdown of all NFs
def on_failure():
    print("Exiting...")
    for p in procs_list:
        p.kill()
    sys.exit(1)

procs_list = []
nf_list = []
cmds_list = []
if __name__ == '__main__':
    # handle cleanup
    signal(SIGINT, handler)

    # get path to config file
    path = get_config()

    # open config file
    with open(path) as f:
        data = json.load(f)
    # parse 
    for k, v in data.items():
        for item in v:
            nf_list.append(k)
            param = str(item).strip("'{[]}'")
            param = remove_prefix(param, "u'parameters': u'")
            cmds_list.append("./go.sh " + param)
    
    cwd = os.getcwd()
    for cmd, nf in zip(cmds_list, nf_list):
        os.chdir(nf)
        procs_list = [Popen(cmd, stdout=PIPE, shell=True)]
        os.chdir(cwd)

    # check that NFs started without errors
    while 1:
        for p in procs_list:
            ret_code = p.poll()
            if ret_code is not None: 
                on_failure()
                break
            else: 
                time.sleep(.1)
                continue