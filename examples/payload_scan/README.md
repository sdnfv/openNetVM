Payload scan
==
The payload scan NF drops/forwards packets based on whether or not a user-input string appears in the packets payload. 

Compilation and Execution
--
```
cd examples
make
cd payload_scan
./go.sh SERVICE_ID -d DESTINATION_ID -s INPUT_STRING

OR 

./go.sh -F CONFIG_FILE -- -- -d DST -s INPUT_STRING [-p PRINT_DELAY] [-i inverse mode]
```

App Specific Arguments
--
  - `-i`: If the user-input string appears in the packets payload, drop the packet instead of forwarding it.
  - `-s <input_string>`: String used to search within a packets payload.
  - `-p <print_delay`: number of packets between each print, e.g. -p 1 prints every packets.

