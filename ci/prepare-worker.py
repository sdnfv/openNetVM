from paramiko import RSAKey
from paramiko import SSHClient
from paramiko import AutoAddPolicy
import sys

if len(sys.argv) != 3:
    print("ERROR! Incorrect number of arguments")
    sys.exit(1)

worker_ip = sys.argv[1]
key_file = sys.argv[2]

key = RSAKey.from_private_key_file(key_file)

client = SSHClient()
client.set_missing_host_key_policy(AutoAddPolicy())

client.connect(worker_ip, timeout = 30, pkey = key)

# put all removals in one line to save execution time
wipe_files = "sudo rm -rf /mnt/huge/* repository *stats* *config* *key* *.py *.sh"
(stdin, stdout, stderr) = client.exec_command(wipe_files)
(stdin, stdout, stderr) = client.exec_command("sudo reboot")
print("Successfully sent {} to reboot".format(worker_ip))

client.close()
