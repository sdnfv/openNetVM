Firewall
==
The Firewall NF is an NF which drops/forwards packets based on LPM rules.

Compilation and Execution
--
```
cd examples
make
cd firewall
./go.sh INSTANCE_ID -d SERVICE_ID -f RULES_FILE

OR

./go.sh INSTANCE_ID -d SERVICE_ID -f RULES_FILE -b 

cd examples
./start_nf firewall INSTANCE_ID -d SERVICE_ID -f RULES_FILE

OR

./start_nf firewall INSTANCE_ID -d SERVICE_ID -f RULES_FILE -b

```

App Specific Arguments
--
  - `-b specifies a debug mode. Prints when packets are dropped/forwarded.
  - `-f specifies a rules file. These are rules used for LPM lookup.

