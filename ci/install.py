import pexpect
import os

cwd = os.path.dirname(os.path.realpath(__file__))
cmd = cwd + "/repository/scripts/install.sh"
child = pexpect.spawn(cmd)

# bind correct interface to DPDK
child.expect("Bind interface 0000:07:00.0 to DPDK.*")
child.sendline("N")
child.expect("Bind interface 0000:07:00.1 to DPDK.*")
child.sendline("Y\n")
child.interact()
