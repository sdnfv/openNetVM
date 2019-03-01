#!/usr/bin/python

from paramiko import RSAKey
from paramiko import SSHClient
from paramiko import AutoAddPolicy
import sys

"""
get buffered stream of input from the client channel while the worker.sh script executes
"""
def line_buffered(f):
    line_buf = ""
    while not f.channel.exit_status_ready():
        line_buf += str(f.read(1))
        if line_buf.endswith('\n'):
            yield line_buf
            line_buf = ''

if len(sys.argv) != 3:
    print("ERROR! Incorrect number of arguments")
    sys.exit(1)

ip = sys.argv[1]
key_file = sys.argv[2]

key = RSAKey.from_private_key_file(key_file)

client = SSHClient()
client.set_missing_host_key_policy(AutoAddPolicy())

client.connect(ip, timeout = 30, pkey = key)

(stdin, stdout, stderr) = client.exec_command("sudo ./worker.sh")

for l in line_buffered(stdout):
    print(l.strip("\n"))

print(str(stderr.read()))
