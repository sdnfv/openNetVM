import pexpect
import os

cwd = os.path.dirname(os.path.realpath(__file__))
cwd += "/repository/scripts"
child = pexpect.spawn(cwd + "/install.sh", cwd=cwd)

# script takes a while, increase timeout
child.timeout = 120
# bind correct interface to DPDK
child.expect("Bind interface 0000:07:00.0 to DPDK.*")
child.sendline("N")
child.expect("Bind interface 0000:07:00.1 to DPDK.*")
child.sendline("Y\n")
child.interact()
