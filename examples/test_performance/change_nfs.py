#!/usr/bin/python

### This script modifies the example_nf_deploy.json file so that it runs (input - 1) simple_forward NFs
### It runs (input -1) simple_forward NFs, 1 speed tester, or (input) total nfs

import json
import sys

num_nfs = 1

if len(sys.argv) == 2:
	num_nfs = int(sys.argv[1])

print(f"did it work-change nfs: {num_nfs}")

with open("../example_nf_deploy.json", "r+") as file:
	json_data = file.read()
data = json.loads(json_data)

### finds the last NF, deletes it, replaces it with modified parameters, adds another NF
while(num_nfs - 1 > len(data["simple_forward"])):
	nf_index_to_change = len(data["simple_forward"]) - 1

	simple_forward_json = data["simple_forward"][nf_index_to_change]["parameters"]
	# print(len(data["simple_forward"]))
	# print(f"simple_forward_json: {simple_forward_json}")
	if (len(data["simple_forward"]) < 8):
		start_nf = simple_forward_json[:-1]
	else:
		start_nf = simple_forward_json[:-1]
	# print(f"start_nf: {start_nf}")
	# print(f"start_nf[0]: {start_nf[0]} is type {type(start_nf[0])}")

	# print(f"str((int(start_nf[0])  +1    ) : {(int(start_nf[0])  +1    )}" )
	if (len(data["simple_forward"]) < 9):
		start_nf = start_nf + str((int(start_nf[0])  +1    )         ) #makes new destination by concatenating the ID of the last nf (to be added)
	else:
		start_nf = start_nf + str((int(start_nf[0:2])  +1    )         )
	# print(f"start nf after concatenation: {start_nf}")

	
	if (len(data["simple_forward"]) < 8):
		end_nf = start_nf[-1] + " -d 1" # causes the last NF to route back to the speed_tester
	else: 
		end_nf = start_nf[-2:] + " -d 1" # causes the last NF to route back to the speed_tester

	# print(f"end_nf: {end_nf}")
	start_nf_dict = {"parameters":start_nf}
	end_nf_dict = {"parameters":end_nf}

	del data["simple_forward"][nf_index_to_change]

	data["simple_forward"].append(start_nf_dict)
	data["simple_forward"].append(end_nf_dict)

new_data = json.dumps(data, indent=2)

with open("../example_nf_deploy_test_p_template.json", "w") as file:
        json.dump(data, file, indent=8)
