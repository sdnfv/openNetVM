Cloudlab Tutorial 
=====================================

Getting Started
-----------------

- This tutorial assumes that you have access to the openNetVM `CloudLab <https://cloudlab.us/>`_ 
    - To gain access, you must first create an account, choose to join a project and enter "GWCloudLab" to request access

Setting up SSH Keys for CloudLab
-----------------

CloudLab allows you to upload an SSH key

- Open a terminal and cd to the directory you want to store the keys in

    Generally, on a Windows machine, all SSH-related files are found in following location: /c/Users/PC_USER_NAME/.ssh/
    
To generate an SSH Key:

- Type :code:`ssh-keygen` to make an ssh-key

- Give it a name and an optional password

- The public key has the extension .pub


To upload the key:

- Go to cloudlab, and click on your username in the top-right corner

- Go to "manage SSH keys"

- Press "Load from file" and select the public key file

- Once it's loaded in, select "add key"

.. image:: ../images/cloudlab-ssh.png
   :width: 600

Starting a ONVM Profile With 1 Node on CloudLab
-----------------

- Click on experiments in the upper-left corner, then start an experiment

- For "select a profile" you can choose the desired profile (onvm is the most updated/recommended)

- For "parameterize" use 1 host, and for node type, you can keep it as is or select a different one. 

    If you're running into trouble runnign the manager, selecting the c220g5 node may assist you
    
- For "finalize" you can just click next

- For "schedule," you don't have to make a schedule, leaving it blank will start it immediately 

Connecting to the Node via VSCode
-----------------

Before connecting, you must have uploaded your SSH key, and started an experiment
You also must have these VSCode extensions:
    Remote - SSH
    Remote - SSH: Editing Configuration Files (may come preinstalled with Remote SSH)
    Remote - Containers
    Remote - WSL

There are 2 easy ways to connect:

1) Via a terminal

- Open a VSCode terminal and cd inside your .ssh folder 

- :code:`ssh -p 22 -i <privatekeyname> <user>@<address>`

- Your <user>@<address> can be found by going to your experiment and clicking on "list view," it should be under "SSH Command"

.. image:: ../images/ssh-connect.png
   :width: 600

2) Via a Remote Window

- Open the "Remote Explorer" via the sidebar (on the left by default)

- In the drop-down window at the top, select SSH Targets

.. image:: ../images/vscode-remote-explorer.png
   :width: 600

- To the right of the SSH Targets bar, click the plus button, and enter :code:`ssh <user>@<address>`

- Select a configuration file (recommanded to use the one in the .ssh folder as mentioned earlier)

- Modify the config file so that it has the correct settings:
    It should have :code:`Port 22` :code:`IdentityFile <privatekeyname>` and :code:`AddKeysToAgent Yes` (on seperate lines)
    
    You can also rename :code:`Host` to whatever you want, but :code:`HostName` must not be changed

.. image:: ../images/config.png
   :width: 600
    
-  If it asks you to choose an operating system, select Linux

Running the ONVM Manager and Speed Tester NF on the node
-----------------
Once you are properly connected to the node, it's time to run the manager

- First, cd into /local/onvm/openNetVM/scripts and run :code:`source setup_cloudlab.sh`

- Depending on which node you're using, it will ask you to bind certain network devices to dpdk 
    In general, you want to make sure that the 2 10 GbE devices are bound (the letters/numbers before listing the device can be used as identifiers)
    
- Go to /local/onvm/openNetVM/onvm and run :code:`make`

- Go to /local/onvm/openNetVM/examples and run :code:`make`
   
- Go to /local/onvm/openNetVM and run sudo :code:`./onvm/go.sh -k 1 -n 0xF8 -s stdout`
    If this gives you an error, it may be an issue with the pre-made profile, and you mmay have to pull a new onvm profile from GitHub in a new directory
    
    Instructions on how to do so can be found `here <https://github.com/sdnfv/openNetVM/blob/master/docs/Install.md/>`_
    
.. image:: ../images/onvm-manager.png
   :width: 600
   
- To run the speed tester, open a new tab while the manager is running and go to /local/onvm/openNetVM/examples/speed_tester 

- run :code:`./go.sh 1 -d 1 -c 16000`

