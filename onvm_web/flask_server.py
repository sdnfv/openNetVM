#!usr/bin/env python3

#                        openNetVM
#                https://sdnfv.github.io
#
# OpenNetVM is distributed under the following BSD LICENSE:
#
# Copyright(c)
#       2015-2017 George Washington University
#       2015-2017 University of California Riverside
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in
#   the documentation and/or other materials provided with the
#   distribution.
# * The name of the author may not be used to endorse or promote
#   products derived from this software without specific prior
#   written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

"""This is the python server that handles onvm web request"""

import os
import subprocess
import socket
from flask import Flask, request, make_response
from flask_cors import CORS

app = Flask(__name__)
cors = CORS(app, resources={r"/*": {"origins": "*", "allow_headers": "*"}})

# pylint: disable=global-at-module-level
global chain_pid_dict, chain_counter
chain_pid_dict = {}
chain_counter = 1


@app.route('/onvm_json_stats.json', methods=['GET'])
def handle_get_json_stats():
    """handle upload stats json file"""
    with open("./onvm_json_stats.json", 'r') as data_f:
        data = data_f.read()
    resp = make_response()
    resp.data = data
    return resp, 200


@app.route('/onvm_json_events.json', methods=['GET'])
def handle_get_json_events():
    """handle upload events json file"""
    with open("./onvm_json_events.json", 'r') as events_f:
        event = events_f.read()
    resp = make_response()
    resp.data = event
    return resp


@app.route('/upload-file', methods=['POST'])
def handle_upload_file():
    """Handle upload file"""
    try:
        file_data = request.files['configFile']
        file_data.save("../examples/nf_chain_config.json")
        return "upload file success", 200
    except KeyError:
        return "upload file failed", 400


@app.route('/start-nf', methods=['POST'])
def handle_start_nf():
    """Handle start nf"""
    global chain_counter, chain_pid_dict
    # check if the config file have been uploaded
    if not os.path.getsize("../examples/nf_chain_config.json"):
        return "config file not uploaded", 400
    # check if the process is already started
    try:
        command = ['python3', '../examples/config.py',
                   '../examples/nf_chain_config.json']
        # start the process
        with open('./nf-chain-logs/log' + str(chain_counter) + '.txt', 'w+') as log_file:
            os.chdir('../examples/')
            p = subprocess.Popen(command, stdout=log_file,
                                 stderr=log_file, universal_newlines=True)
            chain_pid_dict[chain_counter] = [p.pid, 1]
            # change back to web dir for correct output
            os.chdir('../onvm_web/')
            chain_counter += 1
        return "start nf chain success", 200
    except OSError:
        return "start nf chain failed", 500


@app.route('/stop-nf', methods=['POST'])
def handle_stop_nf():
    """Handle stop nf"""
    global chain_pid_dict
    try:
        # get the chain id of which to be stopped
        chain_id = int(request.get_json()['chain_id'])
        # check if the process is already stopped
        if chain_id != 0:
            chain_pid_dict[chain_id][1] = check_is_running(chain_id)
            if chain_pid_dict[chain_id][1] != 0:
                # Stop the chain
                _chain_pid_to_be_stopped = chain_pid_dict[chain_id][0]
                if stop_nf_chain(_chain_pid_to_be_stopped):
                    return "failed to stop nf chain", 500
                del chain_pid_dict[chain_id]
                return "stop nf chain success", 200
            # The chain is already stopped
            del chain_pid_dict[chain_id]
            return "chain already stopped", 400
        result = 1
        for key in chain_pid_dict:
            if check_is_running(key) != 0:
                if stop_nf_chain(chain_pid_dict[key][0]) == 0:
                    result = 0
        chain_pid_dict.clear()
        if result == 0:
            return "failed to stop nf chain", 500
        return "stop nf chain success", 200
    except KeyError:
        return "failed to find the key", 400


def stop_nf_chain(chain_id):
    """Stop the chain with the id"""
    try:
        pid_list = os.popen(
            "ps -ef | awk '{if ($3 == " + str(chain_id) + ") print $2}'")
        pid_list = pid_list.read().split("\n")[:-2]
        pd = pid_list[0]
        temp = os.popen("ps -ef | awk '{if($3 == " + pd + ") print $2}'")
        temp = temp.read().replace("\n", "")
        if temp != "":
            _pid_for_nf = os.popen(
                "ps -ef | awk '{if($3 == " + temp + ") print $2}'")
            _pid_for_nf = _pid_for_nf.read().replace("\n", "")
            print(_pid_for_nf)
            os.system("sudo kill " + _pid_for_nf)
        clear_log(chain_id)
        return 1
    except OSError:
        return 0


def check_is_running(chain_id):
    """Check is the process running"""
    with open('./nf-chain-logs/log' + str(chain_id) + '.txt', 'r') as log_file:
        log = log_file.readline()
        print(log)
        if log is None or log == "":
            return 0
        while log is not None and log != "":
            log_info = log.split(" ")
            if log_info[0] == "Error" or log_info[0] == "Exiting...":
                return 0
            log = log_file.readline()
    return 1


def clear_log(chain_id):
    """Clear the log with the input chain id"""
    file_name = './nf-chain-logs/log' + str(chain_id) + '.txt'
    os.remove(file_name)


if __name__ == "__main__":
    host_name = socket.gethostname()
    host_ip = socket.gethostbyname(host_name)
    app.run(host=host_ip, port=8000, debug=False)
