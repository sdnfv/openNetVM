from flask import Flask, jsonify, request

import requests
from ipaddress import ip_address, ip_network

import hmac
import json
import sys
import pprint
import os
import subprocess

EVENT_URL = "/github-webhook"
CI_NAME="onvm"

app = Flask(__name__)

def verify_request_ip(request):
    src_ip = ip_address(u'{}'.format(request.access_route[0]))
    valid_ips = requests.get('https://api.github.com/meta').json()['hooks']

    for ip in valid_ips:
        if src_ip in ip_network(ip):
            return True

    return False

def verify_request_secret(request):
    header_signature = request.headers.get('X-Hub-Signature')
    if header_signature is None:
        return False

    signature = header_signature.split('=')[1]

    mac = hmac.new(str(secret).encode('utf-8'), msg=request.data, digestmod='sha1')
    if not hmac.compare_digest(mac.hexdigest(), signature):
        return False

    return True


# returns extracted data if it is an event for a PR creation or PR comment creation
# if it is a PR comment, only return extracted data if it contains the required keyword specified by the global var
# if it doesn't contain the keyword or is not the correct type of event, return None
def filter_to_prs_and_pr_comments(json):
    # null check
    if json is None:
        return None

    if json['action'] == 'opened' and 'pull_request' in json and 'base' in json['pull_request']:
        branch_name = json['pull_request']['base']['label']
        repo_name = json['repository']['name']
        user_name = json['pull_request']['user']['login']
        if branch_name is None:
            return None

        try:
            number = json['number']
        except:
            number = None

        return {
            "id": number,
            "repo": repo_name,
            "branch": branch_name,
            "user": user_name,
            "body": "In response to PR creation"
        }

    if json['action'] == 'created' and 'issue' in json and json['issue']['state'] == 'open' and 'pull_request' in json['issue'] and 'comment' in json:
        repo_name = json['repository']['name']
        comment_txt = json['comment']['body']
        user_name = json['comment']['user']['login']

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
            "repo": repo_name,
            "user": user_name,
            "body": comment_txt
        }

    return None

@app.route(EVENT_URL, methods=['POST'])
def init_ci_pipeline():
    
    if not verify_request_ip(request):
        print("Incoming webkooh not from a valid Github address")
        return jsonify({
            "success": True
        })

    if not verify_request_secret(request):
        print("Incoming webhook secret doesn't match configured secret")
        return jsonify({
            "success": True
        })

    extracted_data = filter_to_prs_and_pr_comments(request.json)
    if extracted_data is not None:
        if (extracted_data['user'] in authorized_users):
            print("This is an authorized user")
        else:
            print("This user is not authorized")
            return jsonify({
                "success": True
            })

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
            os.system("./ci_busy.sh config {} \"{}\" \"{}\" \"Another CI run in progress, please try again in 15 minutes\""
                      .format(extracted_data['id'], extracted_data['repo'], extracted_data['body']))
        else:
            os.system("./manager.sh config {} \"{}\" \"{}\"".format(extracted_data['id'], extracted_data['repo'], extracted_data['body']))
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

    if(len(sys.argv) != 5):
        print("Invalid arguments!")
        sys.exit(1)

    host = sys.argv[1]
    port = sys.argv[2]
    KEYWORD = sys.argv[3]

    with open (sys.argv[4], 'r') as cfg:
        webhook_config = json.load(cfg)
    secret = webhook_config['secret']
    authorized_users = webhook_config['authorized-users']
    if secret is None:
        print("No secret found in webhook config")
        sys.exit(1)

    app.run(host=host, port=port)
