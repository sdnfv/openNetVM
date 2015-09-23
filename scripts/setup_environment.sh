#! /bin/bash
set -e

# A script to configure your environment each boot
# Loads the uio kernel modules, shows NIC status

# Ensure we're working relative to the onvm root directory
if [ $(basename $(pwd)) == "scripts" ]; then
    cd ..
fi

# Confirm environment variables
echo "\$RTE_SDK = $RTE_SDK"
echo "\$RTE_TARGET = $RTE_TARGET"

read -r -p "Are these correct? [y/N] " response
if [[ ! $response =~ ^([yY][eE][sS]|[yY])$ ]]; then
    exit 1
fi

start_dir=$(pwd)

# Verify sudo access
sudo -v

# Load uio kernel modules
echo "Loading uio kernel modules"
sleep 1
cd $RTE_SDK/$RTE_TARGET/kmod
sudo modprobe uio
sudo insmod igb_uio.ko

echo "Checking NIC status"
sleep 1
$RTE_SDK/tools/dpdk_nic_bind.py --status

read -r -p "Bind interface p2p1? [y/N] " response
if [[ $response =~ ^([yY][eE][sS]|[yY])$ ]]; then
    echo "Binding p2p1 to dpdk"
    sudo ifconfig p2p1 down
    $RTE_SDK/tools/dpdk_nic_bind.py -b igb_uio 07:00.0
    $RTE_SDK/tools/dpdk_nic_bind.py --status
fi

