from flask import Flask, jsonify, request
import requests

import json
import sys
import pprint
import os
import subprocess

EVENT_URL = "/github-webhook"
CI_NAME="onvm"

app = Flask(__name__)

# returns extracted data if it is an event for a PR creation or PR comment creation
# if it is a PR comment, only return extracted data if it contains the required keyword specified by the global var
# if it doesn't contain the keyword or is not the correct type of event, return None
def filter_to_prs_and_pr_comments(json):
    # null check
    if json is None:
        return None

    if json['action'] == 'opened' and 'pull_request' in json and 'base' in json['pull_request']:
        branch_name = json['pull_request']['base']['label']
        if branch_name is None:
            return None

        try:
            number = json['number']
        except:
            number = None

        return {
            "id": number,
            "branch": branch_name,
            "body": "In response to PR creation"
        }

    if json['action'] == 'created' and 'issue' in json and json['issue']['state'] == 'open' and 'pull_request' in json['issue'] and 'comment' in json:

        comment_txt = json['comment']['body']
        if KEYWORD not in comment_txt:
            return None
        if json['sender']['login'] == CI_NAME:
            return None

        try:
            number = json['issue']['number']
        except:
            number = None

        return {
            "id": number,
            "body": comment_txt
        }

    return None

@app.route(EVENT_URL, methods=['POST'])
def init_ci_pipeline():

    extracted_data = filter_to_prs_and_pr_comments(request.json)
    if extracted_data is not None:
        print("Data matches filter, we should RUN CI")
        print(extracted_data)

        # Check if there is another CI run in progress
        proc1 = subprocess.Popen(['ps', 'cax'], stdout=subprocess.PIPE)
        proc2 = subprocess.Popen(['grep', 'manager.sh'], stdin=proc1.stdout,
                                         stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        proc1.stdout.close()
        out, err = proc2.communicate()

        if (out):
            print("Can't run CI, another CI run in progress")
            os.system("./ci_busy.sh config {} \"{}\" \"Another CI run in progress, please try again in 15 minutes\""
                      .format(extracted_data['id'], extracted_data['body']))
        else:
            os.system("./manager.sh config {} \"{}\"".format(extracted_data['id'], extracted_data['body']))
    else:
        print("Data did not match filter, SKIP CI")

    return jsonify({
        "success": True
    })

@app.route("/status", methods=['GET'])
def status():
    return jsonify({
        "status": "ONLINE"
    })

if __name__ == "__main__":
    global KEYWORD

    if(len(sys.argv) != 4):
        print("Invalid arguments!")
        sys.exit(1)

    host = sys.argv[1]
    port = sys.argv[2]
    KEYWORD = sys.argv[3]

    app.run(host=host, port=port)
