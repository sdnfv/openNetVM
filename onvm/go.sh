# run on cores 0,1,2 (first socket) and 6 (second socket)
# this works well on our 2x6-core nodes
sudo rm -rf /mnt/huge/*
sudo ./onvm_mgr/onvm_mgr/x86_64-native-linuxapp-gcc/onvm_mgr -l 0,1,2,6 -n 3 --proc-type=primary  -- -p1

