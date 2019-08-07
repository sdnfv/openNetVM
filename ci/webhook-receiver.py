from flask import Flask, jsonify, request

import requests
from ipaddress import ip_address, ip_network

from Crypto.PublicKey import RSA
from Crypto.Cipher import AES, PKCS1_OAEP

import hmac
import json
import sys
import pprint
import os
import subprocess
import logging
import time
import threading

# Global vars
EVENT_URL = "/github-webhook"
CI_NAME = "onvm"
KEYWORD = None
access_log_enabled = None
authorized_users = None
secret_file_name = None
private_key_file = None
secret = None
queue_lock = None
request_event = None
ci_request_list = None
manager_running = False

app = Flask(__name__)

logging.getLogger('werkzeug').setLevel(logging.ERROR)
logging.basicConfig(filename="access_log", filemode='a',
                    format='%(asctime)s, %(name)s %(levelname)s %(message)s',
                    datefmt='%d-%b-%y %H:%M:%S', level=logging.INFO)

def get_request_info(request_ctx):
    return "Request details: IP: {}, User: {}, Repo: {}, ID: {}, Body: {}.".format(request_ctx['src_ip'], request_ctx['user'], request_ctx['repo'], request_ctx['id'], request_ctx['body'])

def log_access_granted(request_ctx, custom_msg):
    if (access_log_enabled):
        logging.info("Access GRANTED: {}. {}".format(custom_msg, get_request_info(request_ctx)))

def log_access_denied(request_ctx, custom_msg):
    logging.warning("Access DENIED: {}. {}".format(custom_msg, get_request_info(request_ctx)))

def decrypt_secret():
    global secret_file_name
    global private_key_file_name

    secret_file = open(secret_file_name, "rb")
    private_key = RSA.import_key(open(private_key_file_name).read())

    enc_session_key, nonce, tag, ciphertext = \
       [ secret_file.read(x) for x in (private_key.size_in_bytes(), 16, 16, -1) ]

    # Decrypt the session key with the private RSA key
    cipher_rsa = PKCS1_OAEP.new(private_key)
    session_key = cipher_rsa.decrypt(enc_session_key)

    # Decrypt the data with the AES session key
    cipher_aes = AES.new(session_key, AES.MODE_EAX, nonce)
    data = cipher_aes.decrypt_and_verify(ciphertext, tag)

    # Clear memory
    secret_file = private_key = None
    private_key_file_name = secret_file_name = None
    enc_session_key = nonce = tag = ciphertext = None
    del private_key_file_name
    del secret_file_name
    del secret_file
    del private_key
    del enc_session_key
    del nonce
    del tag
    del ciphertext
    
    return data

def verify_request_ip(request_ctx):
    src_ip = request_ctx['src_ip']
    valid_ips = requests.get('https://api.github.com/meta').json()['hooks']

    for ip in valid_ips:
        if src_ip in ip_network(ip):
            return True

    return False

def verify_request_secret(request_ctx):
    global secret
    header_signature = request_ctx['X-Hub-Signature']
    if header_signature is None:
        return False

    signature = header_signature.split('=')[1]

    mac = hmac.new(secret, msg=request_ctx['data'], digestmod='sha1')
    secret_comparison = hmac.compare_digest(mac.hexdigest(), signature)

    # Memory cleanup
    header_signature = signature = None
    del header_signature
    del signature

    return secret_comparison

# Returns extracted data if it is an event for a PR creation or PR comment creation
# if it is a PR comment, only return extracted data if it contains the required keyword specified by the global var
# if it doesn't contain the keyword or is not the correct type of event, return None
def filter_to_prs_and_pr_comments(json):
    # null check
    if json is None:
        return None

    if 'action' not in json:
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

# run the manager, and rest of CI process
def run_manager(request_ctx):
    # requests are tuples of (context, run mode)
    log_access_granted(request_ctx, "Running CI")
    os.system("./manager.sh config {} \"{}\" \"{}\" {}".format(request_ctx['id'], request_ctx['repo'], request_ctx['body'], request_ctx['mode']))

def request_handler():
    global manager_running
    while True:
        if ci_request_list:
            # we have a request
            manager_running = True
            run_manager(ci_request_list.pop(0))
            # remove flag for other threads
            manager_running = False
        if not ci_request_list:
            # list empty, go to sleep until signaled
            request_event.wait()
            # remove the flag, so we don't get stuck in loop
            request_event.clear()

def add_request(request_ctx):
    ci_request_list.append(request_ctx)
    if not request_event.isSet():
        # wake up event listener, if it was asleep
        request_event.set()

@app.route(EVENT_URL, methods=['POST'])
def init_ci_pipeline():
    run_mode = 0
    request_ctx = filter_to_prs_and_pr_comments(request.json)
    if request_ctx is None:
        logging.debug("Request filter doesn't match request")
        return jsonify({"success": True})
    
    request_ctx['src_ip'] = ip_address(u'{}'.format(request.access_route[0]))
    request_ctx['X-Hub-Signature'] = request.headers.get('X-Hub-Signature')
    request_ctx['data'] = request.data

    if not verify_request_ip(request_ctx):
        print("Incoming webhook not from a valid Github address")
        log_access_denied(request_ctx, "Incoming webhook not from a valid Github address")
        return jsonify({"success": True})

    if not verify_request_secret(request_ctx):
        print("Incoming webhook secret doesn't match configured secret")
        log_access_denied(request_ctx, "Incoming webhook has an invalid secret")
        return jsonify({"success": True})

    if (request_ctx['repo'] == 'openNetVM' and request_ctx['user'] not in authorized_users):
        run_mode = 1 
        # not an authorized user, tell manager to only run linter
        print("Incoming request is from an unathorized user")

    print("Request matches filter, we should RUN CI. {}".format(get_request_info(request_ctx)))

    request_ctx['mode'] = run_mode

    duplicate_req = False

    if (manager_running or ci_request_list):
        busy_msg = "Another CI run in progress, adding request to the end of the list"
        log_access_granted(request_ctx, "CI busy, placing request in queue")
        if ci_request_list:
            # only check for duplicates if list isn't empty
            with queue_lock:
                for req in ci_request_list:
                    # make sure this is the same PR
                    if req['id'] == request_ctx['id'] and req['repo'] == request_ctx['repo']:
                        duplicate_req = True
                        break
            # ending frees the lock
            if duplicate_req:
                # let user know we're not running this request, it's a duplicate
                busy_msg = "Duplicate request already waiting, ignoring message"
        os.system("./ci_busy.sh config {} \"{}\" \"{}\" \"{}\""
                  .format(request_ctx['id'], request_ctx['repo'], request_ctx['body'], busy_msg))

    if not duplicate_req:
        # we are not a duplicate, add to list
        add_request(request_ctx)

    return jsonify({"status": "ONLINE"})

@app.route("/status", methods=['GET'])
def status():
    return jsonify({"status": "ONLINE"})

def parse_config(cfg_name):
    global access_log_enabled
    global secret_file_name
    global private_key_file_name
    global authorized_users

    with open (cfg_name, 'r') as cfg:
        webhook_config = json.load(cfg)

    access_log_enabled = webhook_config['log-successful-attempts']
    if access_log_enabled is None:
        print("Access log switch not specified in the webhook server config")
        sys.exit(1)

    secret_file_name = webhook_config['secret-file']
    if secret_file_name is None:
        print("No secret file found in the webhook server config")
        sys.exit(1)

    private_key_file_name = webhook_config['private-key-file']
    if private_key_file_name is None:
        print("No private key file found in the webhook server config")
        sys.exit(1)

    authorized_users = webhook_config['authorized-users']
    if authorized_users is None:
        print("No authroized users found in webhook server config")
        sys.exit(1)


if __name__ == "__main__":
    if(len(sys.argv) != 5):
        print("Invalid arguments!")
        sys.exit(1)

    host = sys.argv[1]
    port = sys.argv[2]
    KEYWORD = sys.argv[3]
    cfg_name = sys.argv[4]
    
    parse_config(cfg_name)
    secret = decrypt_secret()
    queue_lock = threading.Lock()
    ci_request_list = []

    # create an event handler for requests
    request_event = threading.Event()
    # dedicate thread to check for and run new requests
    poll_request = threading.Thread(target=request_handler, args=[])
    # run as daemon to catch ^C termination
    poll_request.daemon = True
    poll_request.start()

    logging.info("Starting the CI service")
    app.run(host=host, port=port)
