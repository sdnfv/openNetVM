AES Decrypt
==
This is an example NF that decrypts UDP packets, and then forwards them
to a specific NF destination.

The decryption is done with the AES cypher, and it was implemented by
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
cd aes_decrypt
./go.sh CORELIST SERVICE_ID DST [PRINT_DELAY]

OR

sudo ./build/aesdecrypt -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- -d DST [-p PRINT_DELAY]
```

App Specific Arguments
--
  - `-d <dst>`: destination service ID to foward to
  - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.

Example
--
./examples/aes_encrypt/go.sh 5 1 2
./examples/aes_decrypt/go.sh 6 2 3
./examples/bridge/go.sh 7 3
