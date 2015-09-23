#! /bin/bash
set -e

# A script to configure openNetVM
# Expected to be run as scripts/install.sh

read -r -p "Have you configured \$RTE_TARGET and \$RTE_SDK in your environment? [y/N] " response
if [[ ! $response =~ ^([yY][eE][sS]|[yY])$ ]]; then
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
echo "Configuring hugepages"
sleep 1
sudo mkdir -p /mnt/huge
echo "Adding huge fs to /etc/fstab"
sleep 1
sudo sh -c "echo \"huge /mnt/huge hugetlbfs defaults 0 0\" >> /etc/fstab"
echo "Mounting hugepages"
sleep 1
sudo mount -t hugetlbfs nodev /mnt/huge
echo "Creating 1024 hugepages"
sleep 1
sudo sh -c "echo 1024 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages"

# Configure local environment
echo "Configuring environment"
sleep 1
scripts/setup_environment.sh
