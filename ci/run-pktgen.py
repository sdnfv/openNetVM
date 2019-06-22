from paramiko import RSAKey
from paramiko import SSHClient
from paramiko import AutoAddPolicy
import sys

worker_ip = sys.argv[1]
key_file = sys.argv[2]

key = RSAKey.from_private_key_file(key_file)

client = SSHClient()
client.set_missing_host_key_policy(AutoAddPolicy())

client.connect(worker_ip, timeout = 30, pkey = key)

(stdin, stdout, stderr) = client.exec_command("sudo ~/repository/tools/Pktgen/openNetVM-Scripts/run-pktgen.sh 1", get_pty=True)
# block until script finishes
exit_status = stdout.channel.recv_exit_status()

print("Successfully ran pktgen".format(worker_ip))

client.close()
