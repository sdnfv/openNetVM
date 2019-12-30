#!/usr/bin/python3
from github3 import authorize, GitHub, GitHubError

import sys

if len(sys.argv) < 3:
    print("ERROR! Incorrect number of arguments (3 required)")
    sys.exit(1)

# To be run as a separate script to get authorization
# Need to run after installing correct packages

try:
    with open(sys.argv[1], "r") as credsfile:
        data = [x.strip() for x in credsfile.readlines()]
        user = data[0]
        password = data[1]

    note = 'CI ONVM Token'
    note_url = 'http://nimbus.seas.gwu.edu'
    scopes = ['user', 'repo']

    auth = authorize(user, password, scopes, note, note_url)

    with open(sys.argv[2], "w+") as fd:
        fd.write(auth.token + '\n')
        fd.write(str(auth.id))
except GitHubError as error:
        print(error.errors)

