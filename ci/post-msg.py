#!/usr/bin/python

from github3 import login

import json
import sys

REPO_OWNER = None
REPO_NAME = None

if len(sys.argv) != 6:
    print("ERROR!  Incorrect number of arguments")
    sys.exit(1)

with open(sys.argv[1], "r") as credsfile:
    creds = [x.strip() for x in credsfile.readlines()]

if len(creds) != 2:
    print("ERROR: Incorrect number of lines in credentials file!")

username = creds[0]
password = creds[1]

data = json.loads(sys.argv[2], strict=False)
PR_ID = int(data['id'])
REQUEST = data['request']
POST_LINTER_OUTPUT = 'linter' in data
POST_RESULTS = 'results' in data

REPO_OWNER = str(sys.argv[3])
REPO_NAME = str(sys.argv[4])

POST_MSG = sys.argv[5]

gh = login(username, password=password)
if gh is None or str(gh) is "":
    print("ERROR: Could not authenticate with GitHub!")
    sys.exit(1)

issue = gh.issue(REPO_OWNER, REPO_NAME, PR_ID)
if issue is None or str(issue) is "":
    print("ERROR: Could not get PR information from GitHub for PR %d" % int(sys.argv[2]))
    sys.exit(1)

comment_body=""
for line in REQUEST.split('\n'):
    comment_body += "> " + line + "\n"
comment_body += "\n## CI Message\n\n"
comment_body += POST_MSG + "\n"

if POST_RESULTS:
    with open("./results_summary.stats", "r") as f:
        numerical_results = f.read().strip()
    comment_body += numerical_results

if POST_LINTER_OUTPUT:
    linter_output = None
    try:
        with open("./linter-output.txt", "r") as f:
            linter_output = f.read().strip()
    except:
        pass

    if linter_output is not None and linter_output != "":
        comment_body += ("\n\n## Linter Failed\n\n" + linter_output)
    else:
        comment_body += ("\n\n## Linter Passed")

issue.create_comment(comment_body)
