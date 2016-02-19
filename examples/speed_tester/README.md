## TX Speed Tester NF

This program lets you test how fast the manager's TX threads can move packets. It works by creating a batch of packets and then repeatedly sending that batch of packets to itself.

### Execution
```
# with script:
./go.sh CPU ID

# manually:
sudo ./build/speed_tester -c CPUFLAGS -n 4 --proc-type=secondary -- -n CLIENTID -- 
```
