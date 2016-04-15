#! /bin/bash
set -e

# A script to configure your environment each boot
# Loads the uio kernel modules, shows NIC status
# CONFIGURATION (via environment variable):
#  - Ensure that $RTE_SDK and $RTE_TARGET are set (see install docs)
#  - Set ONVM_NIC to the name of the interface you want to bind (default p2p1)
#  - Can set ONVM_NIC_PCI to the PCI address of the interface (default 07:00.0)

# Confirm environment variables
if [ -z "$RTE_TARGET" ]; then
    echo "Please export \$RTE_TARGET"
    exit 1
fi

if [ -z "$RTE_SDK" ]; then
    echo "Please export \$RTE_SDK"
    exit 1
fi

# Ensure we're working relative to the onvm root directory
if [ $(basename $(pwd)) == "scripts" ]; then
    cd ..
fi

start_dir=$(pwd)

# Disable ASLR
sudo sh -c "echo 0 > /proc/sys/kernel/randomize_va_space"

# Setup/Check for free HugePages
hp_size=$(cat /proc/meminfo | grep Hugepagesize | awk '{print $2}')
hp_count="${ONVM_NUM_HUGEPAGES:-1024}"

sudo sh -c "echo $hp_count > /sys/devices/system/node/node0/hugepages/hugepages-${hp_size}kB/nr_hugepages"
hp_free=$(cat /proc/meminfo | grep HugePages_Free | awk '{print $2}')
if [ $hp_free == "0" ]; then
    echo "No free huge pages. Did you try turning it off and on again?"
    exit 1
fi

# Verify sudo access
sudo -v

# Load uio kernel modules
grep -m 1 "igb_uio" /proc/modules | cat
if [ ${PIPESTATUS[0]} != 0 ]; then
    echo "Loading uio kernel modules"
    sleep 1
    cd $RTE_SDK/$RTE_TARGET/kmod
    sudo modprobe uio
    sudo insmod igb_uio.ko
else
    echo "IGB UIO module already loaded."
fi

echo "Checking NIC status"
sleep 1
$RTE_SDK/tools/dpdk_nic_bind.py --status

echo "Binding NIC status"
if [ -z "$ONVM_NIC_PCI" ];then
    for id in $($RTE_SDK/tools/dpdk_nic_bind.py --status | grep -v Active | grep -e "10G" -e "10-Gigabit" | grep unused=igb_uio | cut -f 1 -d " ")
    do
        read -r -p "Bind interface $id to DPDK? [y/N] " response
        if [[ $response =~ ^([yY][eE][sS]|[yY])$ ]];then
            echo "Binding $id to dpdk"
            sudo $RTE_SDK/tools/dpdk_nic_bind.py -b igb_uio $id
        fi
    done
else
    # Auto binding example format: export ONVM_NIC_PCI=" 07:00.0  07:00.1 "
    for nic_id in $ONVM_NIC_PCI
    do
        echo "Binding $nic_id to DPDK"
        sudo $RTE_SDK/tools/dpdk_nic_bind.py -b igb_uio $nic_id
    done
fi

echo "Finished Binding"
$RTE_SDK/tools/dpdk_nic_bind.py --status
