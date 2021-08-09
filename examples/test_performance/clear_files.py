#!/usr/bin/python
### This script resets the data.txt and example_nf_deploy scripts and returns them to their original state

import json

with open("data.txt", 'r+') as f:
        with open("data-copy.txt", "w") as file:
                file.write(f.read())
        f.truncate(0)

with open("../example_nf_deploy_test_p_template.json", "w") as f:
	data = {"globals": [
                {
                        "TTL": 1
                },
                {
                        "directory": "output_files"
                }
        ],
        "simple_forward": [
                {
                        "parameters": "2 -d 1"
                }
        ],
        "speed_tester": [
                {
                        "parameters": "1 -d 2 -l -c 16000"
                }
        ]}
	json.dump(data, f, indent=8)
