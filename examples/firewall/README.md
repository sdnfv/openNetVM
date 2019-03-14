Firewall
==
The Firewall NF is an NF which drops/forwards packets based on LPM rules.

Compilation and Execution
--
```
cd examples
make
cd firewall
./go.sh INSTANCE_ID -d SERVICE_ID

OR

cd examples
./start_nf firewall INSTANCE_ID -d SERVICE_ID
```

App Specific Arguments
--
  - `-b specifies a debug mode. Prints when packets are dropped/forwarded.

