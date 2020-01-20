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

def paramiko_run(key_file, worker_ip, worker_user):
    key = RSAKey.from_private_key_file(key_file)

    client = SSHClient()
    client.set_missing_host_key_policy(AutoAddPolicy())

    client.connect(worker_ip, timeout = 30, username=worker_user, pkey = key)

    # put all removals in one line to save execution time
    wipe_files = "sudo rm -rf /mnt/huge/* repository *stats* *config* *key* *.py *.sh"
    (stdin, stdout, stderr) = client.exec_command(wipe_files)
    (stdin, stdout, stderr) = client.exec_command("sudo reboot")
    print("Successfully sent {0} to reboot".format(worker_ip))

    client.close()

paramiko_run(key_file, worker_ip, worker_user)
