from paramiko import RSAKey
from paramiko import SSHClient
from paramiko import AutoAddPolicy
import sys

if len(sys.argv) != 4:
    print("ERROR! Incorrect number of arguments")
    sys.exit(1)

worker_ip = sys.argv[1]
key_file = sys.argv[2]
worker_user = sys.argv[3]

key = RSAKey.from_private_key_file(key_file)

client = SSHClient()
client.set_missing_host_key_policy(AutoAddPolicy())
client.connect(worker_ip, timeout = 30, username=worker_user, pkey = key)

(stdin, stdout, stderr) = client.exec_command("sudo ~/repository/tools/Pktgen/openNetVM-Scripts/run-pktgen.sh 1", get_pty=True)
# block until script finishes
exit_status = stdout.channel.recv_exit_status()

print("Successfully ran pktgen".format(worker_ip))

client.close()
