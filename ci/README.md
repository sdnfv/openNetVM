# ONVM CI

### Setting up CI
Run a Flask server that listens for new events from github, will get triggered when a new PR is created or when keyword `@onvm` is mentioned.  
```sh
./run_ci.sh 0.0.0.0 8080 @onvm webhook-config.json
```

To run CI tests manually, requires a config file, the github PR ID, request message and a response message.  
```sh
./manager.sh <config file> <pr ID> <repo name> <request msg>
```  

### Usage
**For Github:**  
The CI will automatically run when a new PR is created. The CI can also be called by commenting on a PR and including the `@onvm` keyword. Please be polite when addressing CI.   

Examples:  
 - `Good morning @onvm, what does the performance look like?`  
 - `Hi @onvm Please re run tests`  
 - `@onvm Try again, please`  

### Description
The CI process can be broken into multiple steps:
1. Parse received Github event

    If new PR or '@onvm' keyword in mentioned try to run CI, if CI is already running send a busy message to Github.

2. Config file verification

    Starts the CI process. The config has to follow this structure to pass validation. Invalid config files won't pass this step.
    Config file structure:
    ```
    WORKER_LIST=("WORKER_1_IP WORKER_1_KEY", "WORKER_2_IP WORKER_2_KEY", ...)
    GITHUB_CREDS=path_to_creditential_file
    REPO_OWNER="OWNER_STRING"
    ```

    Config file example:
    ```
    WORKER_LIST=("nimbnode42 nn42_key")
    GITHUB_CREDS=githubcreds
    REPO_OWNER="sdnfv"
    ```

    Webhook json config example
    ```
    {
        "secret-file": "very_special_encrypted_secret_file.bin",
        "private-key-file": "private_key.pem", 
        "log-successful-attempts": true,
        "authorized-users": ["puffin", "penguin", "pcoach"]
    }
    ```

    GITHUB_CREDS file example:
    ```
    bobby_tables
    boby_pwd_111
    ```

3. Install the required dependencies to checkout github PRs and ssh into worker nodes.

    Installs: python3, python3-pip, paramiko, github3.py, pexpect

4. Fetch the code from the github PR

    Runs `clone-and-checkout-pr.py`

5. Run linter on the checked out code

    Runs the `run_linter` function in `helper-manager-functions.sh`

6. Clean up and restart all worker nodes

    Use paramiko to ssh and run `prepare-worker.py`

7. Wait for worker nodes to reboot.

    Ping them until they are up

8. Move the new code to the worker nodes, build openNetVM on worker nodes and run tests

    Use paramiko to ssh and run `run-workload.py`

9. Run modes are supplied to tell the worker which applications to test

     Handle installation with `worker_files/worker.sh` for builds, and setting up manager for performance tests

10. Acquire results from the worker nodes

    Use scp to copy the result stat file from worker

11. Submit results as a comment on github

    Uses the `post-msg.py` script

### Failures
If CI encounters errors it will post a message to Github indicating that it failed.

### Apache Config for GitHub Webhook URL Forwarding within Cluster

```
ProxyPass /onvm-ci/ http://nimbnode44:8080/
ProxyPassReverse /onvm-ci/ http://nimbnode44:8080/

<Location /onvm-ci>
  Require all granted
</Location>
```  
(Also need to setup github webhook to post to **http://nimbus.seas.gwu.edu/onvm-ci/github-webhook**)

### Public and Private CI Runs

CI is now able to accept requests from unauthenticated users. There is a list of Github users in the public project allowed to create a full run. Anyone who is able to view the private `-dev` repository is able to run CI there as well. In `openNetVM`, if a user is not in our list, the linter and branch checks will be executed, ignoring statistics calculations from the worker nodes.

### Setting Up a Connected Worker

Connecting two nodes is useful for measuring statistics with tools like Pktgen and the MTCP stack. There is a bit of setup required to get working connection working. Firstly, an SFP+ 10Gb Intel cable will be required to connect the Network Interface Cards in the two machines. Once this is done, attempt to bring up the correct interfaces for a stable connection. Some debugging might be required:  
- If you don't know which `ifconfig -a` interface is correct, use `ethtool -p <interface name> 120`  
  - This will blink a light on the interface (you have to be next to the machine for this to help)
- Do this on both machines, to find the name of the interfaces that are linked
- Run `sudo ifconfig <interface name> 11.0.0.1/24 up` on the first machine and `sudo ifconfig <interface name> 11.0.0.2/24 up`
  - This will ensure `ping` understands what IP address it is supposed to talk to
- If `ping -I <interface> 11.0.0.2` on the first machine works, great, if not, try changing the IP addresses or viewing `dmesg`

Now that the interfaces are connected, choose which machine will be the CI worker, and which is a helper (Pktgen for example). Install Pktgen on this node by sending the `ci/install_pktgen` files to that machines' home folder. *Remember public keys must be created for all new machines*. Store these public keys in a folder with the server name, see the next section on statistics for more information. Run `chmod +x install-pktgen.sh` if it's not already an executable and run `./install-pktgen.sh` to install everything. If there are dependency errors, the machine might be a different version, so try to install the necessary packages. Once everything is installed, test ONVM->Pktgen between the machines, and if a connection is established, CI should work just fine with no more setup!

### Advanced Statistics

As CI continued to improve, with more programs to test with, benchmarks were made to track the average performance of a worker. In the future, CI will be able to handle multiple workers running many different tests. Since server configurations are not all the same, some with different hardware (Intel x710 vs. x520 NIC for example), performance of the nodes will not be the same. All that matters with CI is that the result of a run is the same or better, not globally across all nodes, but based on the specific server it ran on. For each worker, create a folder in the ci directory with the name of the worker IP. For example if `nimbnode17` is the current worker, a folder with path `/ci/nimbnode17/` should exist. In this folder, 3 files should be there at least. Firstly, a `benchmarks` file (used by the manager) should look similar to this: 

```
AVG_SPEED_TESTER_SPEED=40000000
AVG_PKTGEN_SPEED=10000000
AVG_MTCP_SPEED=.230
```
This is a configuration file, sourced by the manager to keep track of `nimbnode17`'s average performance for each test (currently Speed Tester, Pktgen, and mTCP). The other two files in the folder should be the two public keys, one for the worker, and the second for the worker's client server. Check the previous section on setting up a connection for more information.

### Checking if Online

If you are worried if the CI build is offline or want to make sure it is listening for events, you can check the following url: curl http://nimbus.seas.gwu.edu/onvm-ci/status.  If that URL returns 404, CI is offline.  Otherwise it will display a message saying it is online.

### Scripts
 - `./manager.sh` Is the main script that handles all the creditential validation, setting up workers, running the workload, extracting stats.
 - `webhook-receiver.py` Listens for Github events, grabs the pr info or comment info and checks if we should run CI.
 - `ci_busy.sh` Executed when CI is busy and receives another request. Posts a busy message to Github
 - `post-msg.py` Script that posts all CI messages to Github
 - `prepare-worker.py`, `run-workload.py`, `worker.sh` Scripts for preparing and running workloads on workers.  
Other scripts should be self self-explanatory. 

### Planned improvements
The current version of CI will build and report the speed_tester NF performance. We want to expand CI to cover more complex worker configs and multi worker experiments. A proposed solution is adding a worker config with specific setup and execution instructions into the worker list. With a custom worker config, we can instruct 1 worker to run mTCP epserver and the other node to run apache benchmark to measure the server performance. We can also add other experiments into the list, like multi node chains, specific performance benchmarks, etc. This will require running all the worker experiments in the background without blocking, and for determining when workers have finished the task we can periodically check an output file which the worker will write to when it has finished running the experiment. 
