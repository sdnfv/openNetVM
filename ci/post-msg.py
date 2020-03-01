#!/usr/bin/python

from github3 import login

import json
import sys
from os import path

REPO_OWNER = None
REPO_NAME = None
BASE_BRANCH = 'develop'
ACTION = 'APPROVE'

if len(sys.argv) != 6:
    print("ERROR!  Incorrect number of arguments")
    sys.exit(1)

# add results of statistics file to Github comments
def process_results_from_worker(file, name):
    global comment_body
    global ACTION
    if path.exists(file):
        with open(file) as f:
            results = json.load(f)
        if (results['pass_performance_check']):
            comment_body += " :heavy_check_mark: {} performance check passed\n".format(name)
        else:
            comment_body += " :x: PR drops {} performance below minimum requirement\n".format(name)
            ACTION = 'REQUEST_CHANGES'

# workers have many stats files, make sure its name only displays once
def add_results_from_worker(file):
    global comment_body
    global previous_results_from
    with open(file) as f:
        results = json.load(f)
    if previous_results_from != results['results_from']:
        comment_body += "\n" + results['results_from']
        previous_results_from = results['results_from'] 
    comment_body += "\n" + results['summary']

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
POST_REVIEW = 'review' in data

REPO_OWNER = str(sys.argv[3])
REPO_NAME = str(sys.argv[4])

POST_MSG = sys.argv[5]

gh = login(username, password=password)
if gh is None or str(gh) is "":
    print("ERROR: Could not authenticate with GitHub!")
    sys.exit(1)

pull_request = gh.pull_request(REPO_OWNER, REPO_NAME, PR_ID)
if pull_request is None or str(pull_request) is "":
    print("ERROR: Could not get PR information from GitHub for PR %d" % int(sys.argv[2]))
    sys.exit(1)

previous_results_from = ""
comment_body=""
for line in REQUEST.split('\n'):
    comment_body += "> " + line + "\n"
comment_body += "\n## CI Message\n\n"
comment_body += POST_MSG + "\n"

if POST_REVIEW:
    # PR must be submitted to develop branch
    base_branch_name = pull_request.base.label.split(':')[1].strip()
    if (base_branch_name == BASE_BRANCH):
        comment_body += " :heavy_check_mark: PR submitted to **_%s_** branch\n" %  base_branch_name
    else:
        comment_body += " :x: FIX PR submitted to **_%s_** branch instead of **_%s_**\n" %  (base_branch_name, BASE_BRANCH)
        ACTION = 'REQUEST_CHANGES'

    # PR must not affect performance
    if POST_RESULTS:
        file = './pktgen_summary.stats'
        process_results_from_worker(file, "Pktgen")
        file = './speed_summary.stats'
        process_results_from_worker(file, "Speed Test")

    # PR must pass linter check
    linter_output = None
    try:
        with open("./linter-output.txt", "r") as f:
            linter_output = f.read().strip()
    except:
        pass

    if linter_output is not None and linter_output != "":
        comment_body += " :x: Linter Failed (please fix style errors)\n"
        ACTION = 'REQUEST_CHANGES'
    else:
        comment_body += " :heavy_check_mark: Linter passed\n"

if POST_RESULTS:
    file = './pktgen_summary.stats'
    add_results_from_worker(file)
    file = './speed_summary.stats'
    add_results_from_worker(file)

if POST_LINTER_OUTPUT:
    linter_output = None
    try:
        with open("./linter-output.txt", "r") as f:
            linter_output = f.read().strip()
    except:
        pass

    if linter_output is not None and linter_output != "":
        comment_body += ("\n\n## Linter Output\n\n" + linter_output)

if POST_REVIEW:
    # Actual review is required
    pull_request.create_review(
        body=comment_body,
        event=ACTION
    )
else:
    # Just a general info comment
    pull_request.create_comment(comment_body)
