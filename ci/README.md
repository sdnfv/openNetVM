# ONVM CI

### Setting up CI
Run a Flask server that listens for new events from github, will get triggered when a new PR is created or when keyword `@onvm` is mentioned.  
```sh
python3 webhook-receiver.py 0.0.0.0 8080 @onvm webhook-config.json
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

    Runs the `run_linter` function in `helper-functions.sh`

6. Clean up and restart all worker nodes

    Use paramiko to ssh and run `prepare-worker.py`

7. Wait for worker nodes to reboot.

    Ping them until they are up

8. Move the new code to the worker nodes, build openNetVM on worker nodes and run tests

    Use paramiko to ssh and run `run-workload.py`

9. Acquire results from the worker nodes

    Use scp to copy the result stat file from worker

10. Submit results as a comment on github

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
