AES Encrypt
==
This is an example NF that encrypts UDP packets, and then forwards them
to a specific NF destination.

The encryption is done with the AES cypher, and it was implemented by
Brad Conte :
http://bradconte.com/aes_c
https://github.com/B-Con/crypto-algorithms

The implementation is not optimised and does not use SSE instructions
or any hardware support.

Compilation and Execution
--
```
cd examples
make
cd aes_encrypt
./go.sh SERVICE_ID -d DST [PRINT_DELAY]

OR

./go -F CONFIG_FILE -- -- -d DST [-p PRINT_DELAY]

OR

sudo ./build/aesencrypt -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- -d DST [-p PRINT_DELAY]
```

App Specific Arguments
--
  - `-d <dst>`: destination service ID to foward to
  - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.

Example
--
./examples/aes_encrypt/go.sh 1 -d 2
./examples/aes_decrypt/go.sh 2 -d 3

Config File Support
--
This NF supports the NF generating arguments from a config file. For
additional reading, see [Examples.md](../../docs/Examples.md)

See `../example_config.json` for all possible options that can be set.
