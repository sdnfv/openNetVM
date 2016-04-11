Running openNetVM in Docker
==

Running openNetVM NFs inside Docker containers becomes using the
included [Docker Script][docker].  This script does the following:

  - Creates a Docker container with a custom name
  - Maps NIC devices from the host into the container
  - Maps the shared hugepage region into the container
  - Maps the openNetVM directory into the container
  - Configures the container to use all of the shared memory and data
    structures

Usage
--

To use the script, simply run it from the command line with the
following arguments:

    ```
    sudo ./docker.sh DEVICES HUGEPAGES ONVM NAME
    ```

  - `DEVICES - A comma deliniated list of NIC devices to map to the
    container`
  - `HUGEPAGES - A path to where the hugepage filesystem is on the host`
  - `ONVM - A path to the openNetVM directory on the host filesystem`
  - `NAME - A name to give the container`

    ```
    sudo ./docker.sh /dev/uio0,/dev/uio1 /mnt/huge /root/openNetVM
Basic_Monitor_NF
    ```

  This will start a container with two NIC devices mapped in, /dev/uio0
and /dev/uio1, the hugepage directory at `/mnt/huge` mapped in, and the
openNetVM source directory at `/root/openNetVM` mapped into the
container with the name of Basic_Monitor_NF.

[docker]: ../scripts/docker.sh
