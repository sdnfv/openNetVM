Running openNetVM in Docker
==

To run openNetVM NFs inside Docker containers, use the included [Docker Script][docker].  We provide a [Docker image on Docker Hub][onvm-docker] that is a copy of [Ubuntu 18.04][ubuntu] with a few dependencies installed.  This script does the following:

  - Creates a Docker container off of the [sdnfv/opennetvm Docker image][onvm-docker] with a custom name
  - Maps NIC devices from the host into the container
  - Maps the shared hugepage region into the container
  - Maps the openNetVM directory into the container
  - Maps any other directories you want into the container
  - Configures the container to use all of the shared memory and data structures
  - Depending on the presence of a command to run, the container starts the NF automatically in detached mode or the terminal is connected to it to run the NF by hand

Usage
--

To use the script, simply run it from the command line with the following options:

```bash
sudo ./docker.sh -h HUGEPAGES -o ONVM -n NAME [-D DEVICES] [-d DIRECTORY] [-c COMMAND]
```

  - `HUGEPAGES - A path to where the hugepage filesystem is on the host`
  - `ONVM - A path to the openNetVM directory on the host filesystem`
  - `NAME - A name to give the container`
  - `DEVICES - An optional comma deliniated list of NIC devices to map to the container`
  - `DIRECTORY - An optional additional directory to map inside the container.`
  - `COMMAND - An optional command to run in the container. For example, the path to a go script or to the executable of your NF.`

    ```bash
    sudo ./docker.sh -h /mnt/huge -o /root/openNetVM -n Basic_Monitor_NF -D /dev/uio0,/dev/uio1
    ```

  - This will start a container with two NIC devices mapped in, /dev/uio0 and /dev/uio1, the hugepage directory at `/mnt/huge` mapped in, and the openNetVM source directory at `/root/openNetVM` mapped into the container with the name of Basic_Monitor_NF.

    ```bash
    sudo ./docker.sh -h /mnt/huge -o /root/openNetVM -n Speed_Tester_NF -D /dev/uio0 -c "./examples/start_nf.sh speed_tester 1 -d 1"
    ```

  - This will start a container with one NIC device mapped in, /dev/uio0 , the hugepage directory at `/mnt/huge` mapped in, and the openNetVM source directory at `/root/openNetVM` mapped into the container with the name of Speed_Tester_NF. Also, the container will be started in detached mode (no connection to it) and it will run the go script of the simple forward NF.
Careful, the path needs to be correct inside the container (use absolute path, here the openNetVM directory is mapped in the /).

To remove all containers
```bash
sudo docker rm $(sudo docker ps -aq)
```

To remove all docker images from the system
```bash
# list all images
sudo docker images -a

# remove specific image
sudo docker rmi <IMAGE ID>

# clean up resources not associated with running container
docker system prune

# clean up all resources
docker system prune -a
```

Running NFs Inside Containers
--

To run an NF inside of a container, use the provided script which creates a new container and presents you with its shell.  From there, if you run `ls`, you'll see that inside the root directory, there is an `openNetVM` directory.  This is the same openNetVM directory that is on your host - it is mapped from your local host into the container.  Now enter that directory and go to the example NFs or enter the other directory that you mapped into the container located in the root of the filesystem.  From there, you can run the `go.sh` script to run your NF.

Some prerequisites are:

  - Compile all applications from your local host.  The Docker container is not configured to compile NFs.
  - Make sure that the openNetVM manager is running first on your local host.

Here is an example of starting a container and then running an NF inside of it:

```bash
root@nimbnode /root/openNetVM/scripts# ./docker.sh
sudo ./docker.sh -h HUGEPAGES -o ONVM -n NAME [-D DEVICES] [-d DIRECTORY] [-c COMMAND]

        e.g. sudo ./docker.sh -h /hugepages -o /root/openNetVM -n Basic_Monitor_NF -D /dev/uio0,/dev/uio1
                This will create a container with two NIC devices, uio0 and uio1,
                hugepages mapped from the host's /hugepage directory and openNetVM
                mapped from /root/openNetVM and it will name it Basic_Monitor_NF

root@nimbnode /root/openNetVM/scripts# ./docker.sh -h /mnt/huge -o /root/openNetVM -D /dev/uio0,/dev/uio1 -n basic_monitor
root@899618eaa98c:/openNetVM# ls
CPPLINT.cfg  LICENSE  Makefile  README.md  cscope.out  docs  dpdk examples  onvm  onvm_web  scripts  style  tags  tools
root@899618eaa98c:/openNetVM# cd examples/
root@899618eaa98c:/openNetVM/examples# ls
Makefile  aes_decrypt  aes_encrypt  arp_response  basic_monitor  bridge flow_table  flow_tracker  load_balancer  ndpi_stats  nf_router simple_forward  speed_tester  test_flow_dir
root@899618eaa98c:/openNetVM/examples# cd basic_monitor/
root@899618eaa98c:/openNetVM/examples/basic_monitor# ls
Makefile  README.md  build  go.sh  monitor.c
root@899618eaa98c:/openNetVM/examples/basic_monitor# ./go.sh 3 -d 1
...
```

You can also use the optional command argument to run directly the NF inside of the container, without connecting to it. Then, to stop gracefully the NF (so it has time to notify onvm manager), use the docker stop command before docker rm the container.
The prerequisites are the same as in the case where you connect to the container.

```bash
root@nimbnode /root/openNetVM# ./scripts/docker.sh -h /mnt/huge -o /root/openNetVM -n speed_tester_nf -D /dev/uio0,/dev/uio1 -c "./examples/speed_tester/go.sh 1 -d 1"
14daebeba1adea581c2998eead16ff7ce7fdc45394c0cc5d6489228aad939711
root@nimbnode /root/openNetVM# sudo docker stop speed_tester_nf
speed_tester_nf
root@nimbnode /root/openNetVM# sudo docker rm speed_tester_nf
speed_tester_nf
...
```

Setting Up and Updating Dockerfiles
--

If you need to update the Dockerfile in the future, you will need to follow these steps.

```bash
# install docker fully
sudo curl -sSL https://get.docker.com/ | sh
```

Make an update to `scripts/Dockerfile`. Create an image from the new Dockerfile.

```bash
# run inside scripts/
docker image build -t sdnfv/opennetvm:<some ID tag> - < ./Dockerfile
```
This command may take a while as it grabs the Ubuntu container, and installs dependencies.
Test that the container built correctly. Go into `scripts/docker.sh` and temporarily change line 84

```vim
# from this
sdnfv/opennetvm \
# to this
sdnfv/opennetvm:<some ID tag> \
```

Make sure it is the same tag as the build command. This stops docker from pulling the real `sdnfv/opennetvm`

Test what you need to for the update and remove all containers.
```bash
sudo docker rm $(sudo docker ps -aq)
```

Create an account on Docker online and sign via CLI:
```bash
sudo docker login -u <username> docker.io
```

Make sure you are apart of the sdnfv Docker organization:
```bash
# push updated image
docker push sdnfv/opennetvm

# rename to update latest as well
docker tag sdnfv/opennetvm:<some ID tag> sdnfv/opennetvm
docker push sdnfv/opennetvm:latest
```

Now the image is updated, and will be the default next time someone pulls.

Older Dockerfiles
--

If you want to use an older ONVM version on Ubuntu 14, take a look at the [Available Tags][onvm-docker-tags].
The 18.03 tag runs ONVM when it had been set up for an older version of Ubuntu.
The `latest` dockerfile runs on Ubuntu 20.04 and is called `latest`.

[docker]: ../scripts/docker.sh
[onvm-docker]: https://hub.docker.com/r/sdnfv/opennetvm/
[onvm-docker-tags]: https://hub.docker.com/r/sdnfv/opennetvm/tags
[ubuntu]: http://releases.ubuntu.com/14.04/

