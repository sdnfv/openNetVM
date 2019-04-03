Firewall
==
The Firewall NF drops/forwards packets based on LPM rules specified in the  rules.json file.
A user would enter a rule in the following format:

````
"ruleName": {
		"ip": "127.1.1.0",
		"depth": 32,
		"action": 0
	}
````
Compilation and Execution
--
```
cd examples
make
cd firewall
./go.sh SERVICE_ID -d DESTINATION_ID -f RULES_FILE

OR 

./go.sh -F CONFIG_FILE -- -- -d DST -f RULES_FILE [-p PRINT_DELAY] [-b debug mode]
```

App Specific Arguments
--
  - `-b`: specifies debug mode. Prints individual packet source ip addresses.
  - `-f <rules_file>`: rules used for LPM lookup.
  - `-p <print_delay`: number of packets between each print, e.g. -p 1 prints every packets.

