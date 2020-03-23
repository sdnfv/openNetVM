Simple Forward with Token Bucket
==
Example NF that simulates a queue with a token bucket and forwards packets to a specific destination.

Compilation and Execution
--

```
cd examples
make
cd simple_fwd_tb
```
```
./go.sh SERVICE_ID -d DST [-p PRINT_DELAY] [-D TOKEN_BUCKET_DEPTH] [-R TOKEN_BUCKET_RATE]

OR

./go.sh -F CONFIG_FILE -- -- -d DST [-p PRINT_DELAY] [-D TOKEN_BUCKET_DEPTH] [-R TOKEN_BUCKET_RATE]

OR

sudo ./build/simple_fwd_tb -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- -d DST [-p PRINT_DELAY] [-D TOKEN_BUCKET_DEPTH] [-R TOKEN_BUCKET_RATE]
```

App Specific Arguments
--
  - `-d <dst>`: destination service ID to foward to
  - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.
  - `-D <token_bucket_depth>`: depth of token bucket (in bytes)
  - `-R <token_bucket_rate>`: rate of token regeneration (in mbps)

Config File Support
--
This NF supports the NF generating arguments from a config file. For additional reading, see [Examples.md](../../docs/Examples.md)

See `../example_config.json` for all possible options that can be set.
