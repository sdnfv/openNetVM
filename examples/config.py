import sys
import json
import os
import shlex
import time
from signal import signal, SIGINT, SIGTERM
from subprocess import Popen, PIPE
import subprocess

# cleanup on user shutdown
def handler(signal_received, frame):
    print("Exiting...")
    for p in procs_list:
        try: 
            p.kill()
        except OSError:
            pass
    sys.exit(0)

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

# handle shutdown of all NFs on error
def on_failure(err):
    print(err)
    for proc in procs_list:
        try:
            proc.kill()
        except OSError:
            pass
    for nf in nf_list:
        try: 
            os.system("sudo pkill " + nf)
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
            param = str(item).strip("'{[]}'")
            param = remove_prefix(param, "u'parameters': u'")
            cmds_list.append("./go.sh " + param)
    
    # start NFs 
    cwd = os.getcwd()
    for cmd, nf in zip(cmds_list, nf_list):
        os.chdir(nf)
        p = subprocess.Popen(shlex.split(cmd), stdout=PIPE, stderr=PIPE)
        procs_list.append(p)
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