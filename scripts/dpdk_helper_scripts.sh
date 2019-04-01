#!/bin/bash

HUGEPGSZ=`cat /proc/meminfo  | grep Hugepagesize | cut -d : -f 2 | tr -d ' '`

#
# Unloads igb_uio.ko.
#
remove_igb_uio_module()
{
    echo "Unloading any existing DPDK UIO module"
    /sbin/lsmod | grep -s igb_uio > /dev/null
    if [ $? -eq 0 ] ; then
        sudo /sbin/rmmod igb_uio
    fi
}

#
# Creates hugepage filesystem.
#
create_mnt_huge()
{
    echo "Creating /mnt/huge and mounting as hugetlbfs"
    sudo mkdir -p /mnt/huge

    grep -s '/mnt/huge' /proc/mounts > /dev/null
    if [ $? -ne 0 ] ; then
        sudo mount -t hugetlbfs nodev /mnt/huge
    fi
}

#
# Removes hugepage filesystem.
#
remove_mnt_huge()
{
    echo "Unmounting /mnt/huge and removing directory"
    grep -s '/mnt/huge' /proc/mounts > /dev/null
    if [ $? -eq 0 ] ; then
        sudo umount /mnt/huge
    fi

    if [ -d /mnt/huge ] ; then
        sudo rm -R /mnt/huge
    fi
}

#
# Removes all reserved hugepages.
#
clear_huge_pages()
{
    echo > .echo_tmp
    for d in /sys/devices/system/node/node? ; do
        echo "echo 0 > $d/hugepages/hugepages-${HUGEPGSZ}/nr_hugepages" >> .echo_tmp
    done
    echo "Removing currently reserved hugepages"
    sudo sh .echo_tmp
    rm -f .echo_tmp

    remove_mnt_huge
}

#
# Creates hugepages on specific NUMA nodes.
#
set_numa_pages()
{
    echo "Setting up hugepages"
    sleep 1
    set +e

    clear_huge_pages

    hp_count="${ONVM_NUM_HUGEPAGES:-1024}"

    echo > .echo_tmp
    for d in /sys/devices/system/node/node? ; do
        node=$(basename $d)
        echo "echo $hp_count > $d/hugepages/hugepages-${HUGEPGSZ}/nr_hugepages" >> .echo_tmp
    done
    echo "Reserving hugepages"
    sudo sh .echo_tmp
    rm -f .echo_tmp

    create_mnt_huge

    hp_free=$(cat /proc/meminfo | grep HugePages_Free | awk '{print $2}')
    if [ $hp_free == "0" ]; then
        echo "Coudn't set up huge pages. Did you try turning it off and on again?"
        exit 1
    else
        echo "Huge pages successfully configured"
    fi

    set -e
}
