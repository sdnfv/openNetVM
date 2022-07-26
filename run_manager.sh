#!/usr/bin/bash
cd /local/onvm/openNetVM/scripts
source ./setup_cloudlab.sh
cd /local/onvm/openNetVM
cd onvm && make && cd ..
cd examples && make && cd ..
./onvm/go.sh -k 3 -n 0xFF -s stdout
