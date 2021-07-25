###This script modifies the example_nf_deploy.json file so that it adds an additional simple_forward nf

import json

#num_nfs = input("Enter the number of nfs: ")

with open("../example_nf_deploy.json", "r+") as file:
	json_data = file.read()
data = json.loads(json_data)

### finds the last NF, deletes it, replaces it with modified parameters, adds another NF
nf_index_to_change = len(data["simple_forward"]) - 1

simple_forward_json = data["simple_forward"][nf_index_to_change]["parameters"]

start_nf = simple_forward_json[:-1]
start_nf = start_nf + str((int(start_nf[0])  +1    )         ) #makes new destination by concatenating the ID of the last nf (to be added)

end_nf = start_nf[-1] + " -d 1" # causes the last NF to route back to the speed_tester

start_nf_dict = {"parameters":start_nf}
end_nf_dict = {"parameters":end_nf}


del data["simple_forward"][nf_index_to_change]

data["simple_forward"].append(start_nf_dict)
data["simple_forward"].append(end_nf_dict)

new_data = json.dumps(data, indent=2)

with open("../example_nf_deploy.json", "w") as file:
        json.dump(data, file, indent=8)
