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

nic_list ()
{
    echo "p2p1 p2p2"
}

for nic in $(nic_list)
do
    echo $nic
    if [ "$nic" == "p2p1" ];then
        nic_name=${ONVM_NIC:-p2p1}
        nic_id=${ONVM_NIC_PCI:-07:00.0}
    else
        nic_name=${ONVM_NIC:-p2p2}
        nic_id=${ONVM_NIC_PCI:-07:00.1}
    fi
    read -r -p "Bind interface $nic_name with address $nic_id? [y/N] " response
    if [[ $response =~ ^([yY][eE][sS]|[yY])$ ]]; then
        echo "Binding $nic_name to dpdk"
        sudo ifconfig $nic_name down || true
        sudo $RTE_SDK/tools/dpdk_nic_bind.py -b igb_uio $nic_id
        $RTE_SDK/tools/dpdk_nic_bind.py --status
    fi
done
