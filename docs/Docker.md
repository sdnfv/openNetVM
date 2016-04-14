Running openNetVM in Docker
==

Running openNetVM NFs inside Docker containers becomes using the
included [Docker Script][docker].  This script does the following:

  - Creates a Docker container with a custom name
  - Maps NIC devices from the host into the container
  - Maps the shared hugepage region into the container
  - Maps the openNetVM directory into the container
  - Maps any other directories you want into the container
  - Configures the container to use all of the shared memory and data
    structures

Usage
--

To use the script, simply run it from the command line with the
following arguments:

    ```
    sudo ./docker.sh DEVICES HUGEPAGES ONVM NAME [DIRECTORY]
    ```

  - `DEVICES - A comma deliniated list of NIC devices to map to the
    container`
  - `HUGEPAGES - A path to where the hugepage filesystem is on the host`
  - `ONVM - A path to the openNetVM directory on the host filesystem`
  - `NAME - A name to give the container`
  - `DIRECTORY - An optional directory to be mapped into the container`

    ```
    sudo ./docker.sh /dev/uio0,/dev/uio1 /mnt/huge /root/openNetVM
Basic_Monitor_NF
    ```

  - This will start a container with two NIC devices mapped in, /dev/uio0
and /dev/uio1, the hugepage directory at `/mnt/huge` mapped in, and the
openNetVM source directory at `/root/openNetVM` mapped into the
container with the name of Basic_Monitor_NF.

Running NFs Inside Containers
--

To run an NF inside of a container, use the provided script which creates a new container and presents you with its shell.  From there, if you run `ls`, you'll see that inside the root directory, there is an `openNetVM` directory.  This is the same openNetVM directory that is on your host - it is mapped from your local host into the container.  Now enter that directory and go to the example NFs or enter the other directory that you mapped into the container located in the root of the filesystem.  From there, you can run the `go.sh` script to run your NF.

Some prerequisites are:

  - Compile all applications from your local host.  The Docker container is not configured to compile NFs.
  - Make sure that the openNetVM manager is running first on your local host.

Here is an example of starting a container and then running an NF inside of it:

```
root@nimbnode /root/openNetVM # ./scripts/docker.sh
sudo ./docker.sh DEVICES HUGEPAGES ONVM NAME [DIRECTORY]

  e.g. sudo ./docker.sh /dev/uio0,/dev/uio1 /hugepages /root/openNetVM Basic_Monitor_NF
    This will create a container with two NIC devices, uio0 and uio1,
    hugepages mapped from the host's /hugepage directory and openNetVM
    mapped from /root/openNetVM and it will name it Basic_Monitor_NF

root@nimbnode /root/openNetVM # ./scripts/docker.sh /dev/uio0,/dev/uio1 /hugepages /home/neel/openNetVM ExampleNF
root@2f20c33f9d69:/# ls
bin  boot  dev  etc  home  hugepages  lib  lib64  media  mnt  openNetVM  opt  proc  root  run  sbin  srv  sys  tmp  usr  var
root@2f20c33f9d69:/# cd openNetVM/
root@2f20c33f9d69:/openNetVM# ls
CPPLINT.cfg  LICENSE  Makefile  README.md  docs  dpdk  examples  onvm  scripts  style  tags
root@2f20c33f9d69:/openNetVM# cd examples/
root@2f20c33f9d69:/openNetVM/examples# ls
Makefile  basic_monitor  bridge  flow_table  simple_forward  speed_tester  test_flow_dir
root@2f20c33f9d69:/openNetVM/examples# cd basic_monitor/
root@2f20c33f9d69:/openNetVM/examples/basic_monitor# ls
Makefile  README.md  basic_monitor  build  go.sh  monitor.c
root@2f20c33f9d69:/openNetVM/examples/basic_monitor# ./go.sh
./go.sh [cpu-list] [Service ID] [PRINT]
./go.sh 3 0 --> core 3, Service ID 0
./go.sh 3,7,9 1 --> cores 3,7, and 9 with Service ID 1
./go.sh 3,7,9 1 1000 --> cores 3,7, and 9 with Service ID 1 and Print Rate of 1000
root@2f20c33f9d69:/openNetVM/examples/basic_monitor# ./go.sh 3 1
...
```

[docker]: ../scripts/docker.sh
