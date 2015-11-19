#! /bin/bash
set -e

# A script to configure openNetVM
# Expected to be run as scripts/install.sh
# CONFIGURATION (via environment variable):
#  - Make sure $RTE_TARGET and $RTE_SDK are correct (see install docs)
#  - Set $ONVM_NUM_HUGEPAGES to control the number of pages created
#  - Set $ONVM_SKIP_FSTAB to not add huge fs to /etc/fstab

if [ -z "$RTE_TARGET" ]; then
    echo "Please export \$RTE_TARGET"
    exit 1
fi

if [ -z "$RTE_SDK" ]; then
    echo "Please export \$RTE_SDK"
    exit 1
fi

# Validate sudo access
sudo -v

# Ensure we're working relative to the onvm root directory
if [ $(basename $(pwd)) == "scripts" ]; then
    cd ..
fi

# Set state variables
start_dir=$(pwd)

# Compile dpdk
cd $RTE_SDK
echo "Compiling and installing dpdk in $RTE_SDK"
sleep 1
make config T=$RTE_TARGET
make T=$RTE_TARGET
make install T=$RTE_TARGET

# Refresh sudo
sudo -v

cd $start_dir

# Configure HugePages
hp_size=$(cat /proc/meminfo | grep Hugepagesize | awk '{print $2}')
hp_count="${ONVM_NUM_HUGEPAGES:-1024}"
echo "Configuring $hp_count hugepages with size $hp_size"
sleep 1
sudo mkdir -p /mnt/huge

# Only add to /etc/fstab if user wants it
if [ -z "$ONVM_SKIP_FSTAB" ]; then
    echo "Adding huge fs to /etc/fstab"
    sleep 1
    sudo sh -c "echo \"huge /mnt/huge hugetlbfs defaults 0 0\" >> /etc/fstab"
fi

echo "Mounting hugepages"
sleep 1
sudo mount -t hugetlbfs nodev /mnt/huge
echo "Creating $hp_count hugepages"
sleep 1
sudo sh -c "echo $hp_count > /sys/devices/system/node/node0/hugepages/hugepages-${hp_size}kB/nr_hugepages"

# Configure local environment
echo "Configuring environment"
sleep 1
scripts/setup_environment.sh
