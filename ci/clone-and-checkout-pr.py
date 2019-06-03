#!/usr/bin/python

from github3 import login
import pexpect
import json
import sys

if len(sys.argv) != 5:
    print("ERROR! Incorrect number of arguments")
    sys.exit(1)

REPO_OWNER = sys.argv[3]
REPO_NAME = sys.argv[4]

with open(sys.argv[1], "r") as credsfile:
    creds = [x.strip() for x in credsfile.readlines()]

if len(creds) != 2:
    print("ERROR: Incorrect number of lines in credentials file!")

username = creds[0]
password = creds[1]

sys.argv[2] = json.loads(sys.argv[2])['id']

gh = login(username, password=password)
if gh is None or str(gh) is "":
    print("ERROR: Could not authenticate with GitHub!")
    sys.exit(1)

pr = gh.pull_request(REPO_OWNER, REPO_NAME, int(sys.argv[2]))
if pr is None or str(pr) is "":
    print("ERROR: Could not get PR information from GitHub for PR %d" % int(sys.argv[2]))
    sys.exit(1)

branch_user = pr.head.label.split(":")[0]
branch_name = pr.head.label.split(":")[1]

repo = gh.repository(branch_user, REPO_NAME)

cmd = "git clone " + str(repo.clone_url) + " repository"

child = pexpect.spawn(cmd)

if '-dev' in REPO_NAME:
    child.expect("Username.*")
    child.sendline(username + "\n")
    child.expect("Password.*")
    child.sendline(password + "\n")

child.interact()

# Get the latest branches for upstream (used for proper linter check)
pexpect.run("git remote add upstream https://github.com/sdnfv/openNetVM.git", cwd="./repository")
pexpect.run("git fetch upstream", cwd="./repository")

print(pexpect.run("git checkout " + branch_name, cwd="./repository"))
