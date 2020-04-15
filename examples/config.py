import json
import os
import sys
from multiprocessing import Process
import sys

# no config file passed as arg
if (len(sys.argv) < 2):
    print ("Error: Specify config file")
    sys.exit(0)

# get path of config file 
fn = sys.argv[1]
if os.path.exists(fn):
    path = os.path.join(os.path.abspath(os.getcwd()), os.path.basename(fn))
    # print path
else:
    print ("Error: No config file found")
    sys.exit(0)

# open config file and get num nf's
with open(path) as f:
    data = json.load(f)
    # size = len(data)

# strip parameters
def remove_prefix(text, prefix):
    if text.startswith(prefix):
        return text[len(prefix):]
    return text

#start specified nf
def start_nf(nf, param):
    os.chdir(nf)
    cmd = "./go.sh " + param
    print(nf + ": " + cmd)
    os.system(cmd)
    os.chdir("/local/onvm/openNetVM/examples")
    return

jobs = []
# clean up dict items and get params
for k, v in data.items():
    nf = k
    param = str(v).strip("'{[]}'")
    param = remove_prefix(param, "u'parameters': u'")
    p = Process(target = start_nf, args = (nf, param))
    jobs.append(p)

# start up all nf's
for j in jobs:
    j.start()