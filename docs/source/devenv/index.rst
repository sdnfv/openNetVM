Development Environment
=====================================

`Visual Studio Code Remote Development <https://code.visualstudio.com/docs/remote/remote-overview>`_ allows contributors to use a consistent environment, access a development environment from multiple machines, and use runtimes not available on one's local OS. Their remote development platform can be configured with the Nimbus Cluster and CloudLab for development environments.

Getting Started
-----------------
- Download `Visual Studio Code <https://code.visualstudio.com/download)>`_ 
- Download the Remote Development Extension Pack which includes the 'Remote-SSH', 'Remote-WSL', and 'Remote-Containers' extensions:
  - From `Visual Studio Marketplace <https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.vscode-remote-extensionpack>`_ or 
  - Launch VSCode
    - Click on the 'Extensions' icon on the left sidebar (:code:`Shift + Command + X`)
    - Search 'Remote Development' and install the 'Visual Studio Remote Development Extension Pack'
- Reload VS Code after installation is complete 
- Setup and install OpenSSH on your local machine if one is not already present
- Setup your work environment with CloudLab and/or the Nimbus Cluster 
- Setup Live Share so you can join and host real-time collaboration sessions

Setup OpenSSH
---------------
- Install `OpenSSH <https://code.visualstudio.com/docs/remote/troubleshooting#_installing-a-supported-ssh-client>`_ if not already present on your machine
- Ensure that you have the :code:`.ssh` directory in your user's home directory:

.. code-block:: bash
    :linenos:

    $ cd /Users/[Username]/.ssh
    
- If the directory does not exist on your machine, create it with the following command: 

.. code-block:: bash
    :linenos:
    
    $ mkdir -p ~/.ssh && chmod 700 ~/.ssh
    
- If the :code:`config` file does not yet exist in your :code:`.ssh` directory, create it using command: 

.. code-block:: bash
    :linenos:

    $ touch ~/.ssh/config

- Ensure your :code:`config` file has the correct permisisons using command: 

.. code-block:: bash
    :linenos:

    $ chmod 600 ~/.ssh/config


CloudLab Work Environment
---------------------------
- Setup your `SSH public key <https://help.github.com/en/github/authenticating-to-github/generating-a-new-ssh-key-and-adding-it-to-the-ssh-agent>`_ if you have not already done so
- Login to your CloudLab account and select 'Manage SSH Keys' under your account profile and add your public key
- When you instantiate a CloudLab experiment, a unique SSH command will be generated for you in the form: :code:`ssh -p portNum user@hostname` listed under **Experiment -> List View -> SSH Command** 
- Ensure that your generated ssh command works by running it in terminal 
- Navigate to your system's :code:`.ssh` directory: 

.. code-block:: bash
    :linenos:

    $ cd /Users/[Username]/.ssh

and modify the :code:`config` file to include (**macOS** or **Linux**) : 

.. code-block:: bash
    :linenos:

    Host hostname
      HostName hostname
      Port portNum
      ForwardX11Trusted yes
      User user_name
      IdentityFile ~/.ssh/id_rsa
      UseKeyChain yes
      AddKeysToAgent yes

or (**Windows**) :

.. code-block:: bash
    :linenos:

    Host hostname
      HostName hostname
      Port portNum
      User user_name
      IdentityFile ~/.ssh/id_rsa
      AddKeysToAgent yes

- Select 'Remote-SSH: Connect to Host' and enter :code:`ssh -p portNum user@hostname` when prompted
- VS Code will automatically connect and set itself up
  - See `Troubleshooting tips <https://code.visualstudio.com/docs/remote/troubleshooting#_troubleshooting-hanging-or-failing-connections>`_ for connection issues and `Fixing SSH file permissions <https://code.visualstudio.com/docs/remote/troubleshooting#_fixing-ssh-file-permission-errors>`_ for permissions errors
- After the connection is complete, you will be in an empty window and can then navigate to any folder or workspace using **File -> Open** or **File -> WorkSpace** 
- To initialize and run openNetVM, select **File -> Open** and navigate to :code:`/local/onvm/openNetVM/scripts`
  - Select **Terminal -> New Terminal** and run:
.. code-block:: bash
    :linenos:
    
     $ source setup_cloudlab.sh  
     $ sudo ifconfig ethXXX down
     $ ./setup_environment.sh

where ethXXX is the NIC(s) you would like to bind to DPDK
- To disconnect from a remote host, select **File -> Close Remote Connection** or exit VS Code 

Nimbus Cluster Work Environment 
--------------------------------

Nimbus VPN Method
^^^^^^^^^^^^^^^^^^
- In order to connect directly to your node in the Nimbus Cluster through VS Code, you must be connected to the `Nimbus VPN <http://nimbus.seas.gwu.edu/vpn/>`_
- Navigate to your system's :code:`.ssh` directory: 

.. code-block:: bash
    :linenos:

    $ cd /Users/[Username]/.ssh

and modify the :code:`config` file to include: 

.. code-block:: bash
    :linenos:

    Host nimbnodeX
      Hostname nimbnodeX
      Username user_name

 where 'X' is the node assigned by a Nimbus Cluster system administrator 

- Launch VS Code and click on the green icon on the lower lefthand corner to open a remote window
- Select 'Remote-SSH: Connect to Host' and enter :code:`user@nimbus.seas.gwu.edu` when prompted
- VSCode will automatically connect and set itself up
  - See `Troubleshooting tips <https://code.visualstudio.com/docs/remote/troubleshooting#_troubleshooting-hanging-or-failing-connections>`_ for connection issues and `Fixing SSH file permissions <https://code.visualstudio.com/docs/remote/troubleshooting#_fixing-ssh-file-permission-errors>`_ for permissions errors
- After the connection is complete, you will be in an empty window and can then navigate to any folder or workspace using **File -> Open** or **File -> Workspace** 
- To disconnect from a remote host, select **File -> Close Remote Connection** or exit VS Code 

Alternative Method
^^^^^^^^^^^^^^^^^^^
You can also connect to the Nimbus Cluster through VS Code without using the Nimbus VPN. For instructions on how to configure this, see below.

If working with macOS or Linux:
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
- Navigate to your system's :code:`.ssh` directory: 

.. code-block:: bash
    :linenos:

    $ cd /Users/[Username]/.ssh

and modify the :code:`config` file to include: 

.. code-block:: bash
    :linenos:
    
      Host nimbnodeX
        Username user_name
        ProxyCommand ssh -q user_name@nimbus.seas.gwu.edu nc -q0 %h 22

 where 'X' is the node assigned by a Nimbus Cluster system administrator 

If working with Windows:
^^^^^^^^^^^^^^^^^^^^^^^^^
- Navigate to your system's :code:`.ssh` directory: 

.. code-block:: bash
    :linenos:

    $ cd /Users/[Username]/.ssh

and modify the :code:`config` file to include: 

.. code-block:: bash
    :linenos:

    Host nimbnodeX
      Username user_name
      ProxyCommand C:\Windows\System32\OpenSSH\ssh.exe -q user_name@nimbus.seas.gwu.edu nc -q0 %h 22

 where 'X' is the node assigned by a Nimbus Cluster system administrator 

**Next:**
- Launch VS Code and click on the green icon on the lower lefthand corner to open a remote window
- Select 'Remote-SSH: Connect to Host' and select the host you added, :code:`nimbnodeX`, when prompted
- VSCode will automatically connect and set itself up
  - See `Troubleshooting tips <https://code.visualstudio.com/docs/remote/troubleshooting#_troubleshooting-hanging-or-failing-connections>`_ for connection issues and `Fixing SSH file permissions <https://code.visualstudio.com/docs/remote/troubleshooting#_fixing-ssh-file-permission-errors>`_ for permissions errors
- After the connection is complete, you will be in an empty window and can then navigate to any folder or workspace using **File -> Open** or **File -> Workspace** 
- To disconnect from a remote host, select **File -> Close Remote Connection** or exit VS Code 

cpplint Setup
---------------
- `Linting <https://code.visualstudio.com/docs/python/linting>`_ extensions run automatically when you save a file. Issues are shown as underlines in the code editor and in the *Problems* panel   
- Install cpplint:
  - From `source <https://github.com/cpplint/cpplint>`_ or
  - Mac & Linux: 
.. code-block:: bash
    :linenos:
     $ sudo pip install cpplint 
     
  - Windows:
.. code-block:: bash
    :linenos:
    
     $ pip install cpplint 
     
- Install the cpplint extension
  - From `Visual Studio Marketplace <https://marketplace.visualstudio.com/items?itemName=mine.cpplint&ssr=false#overview>`_ or
  - Launch VSCode
    - Click on the 'Extensions' icon on the left sidebar (:code:`Shift + Command + X`)
    - Search 'cpplint' and install
      
Live Share
------------
Visual Studio `Live Share <https://code.visualstudio.com/blogs/2017/11/15/live-share>`_ allows developers to collaboratively edit in real-time through collaboration sessions. 

- Install the Live Share extension: 
  - From `Visual Studio Marketplace <https://marketplace.visualstudio.com/items?itemName=MS-vsliveshare.vsliveshare-pack>`_ or 
  - Launch VSCode
    - Click on the 'Extensions' icon on the left sidebar (:code:`Shift + Command + X`)
    - Search 'Live Share Extension Pack' and install 
- **Note**: even if you already have the Live Share extension installed in your local VSCode application, you will have to reinstall it in your remote development environment in order to host collaboration sessions while you are connected to CloudLab or the Nimbus Cluster
- Reload VSCode after installation is complete 
- **Note**: Linux users may need to follow extra `installation steps <https://docs.microsoft.com/en-us/visualstudio/liveshare/use/vscode>`_ to configure Live Share
- In order to join or host collaboration sessions, you must sign into Visual Studio Live Share with a Microsoft or GitHub account 
  - To sign in, click on the blue 'Live Share' status bar item on the bottom of the window or press :code:`Ctrl + Shift + P/ Cmd + Shift + P` and select 'Live Share: Sign in with Browser' and proceed to sign in 
- To learn about more features that Live Share provides, see the `User Guide <https://docs.microsoft.com/en-us/visualstudio/liveshare/use/vscode>`_

Collaboration Sessions
------------------------
To edit and share your code with other collaborators in real-time, you can start or join a collaboration session

- To start a session, launch VSCode and click the 'Live Share' status bar on the bottom of the window or press :code:`Ctrl + Shift + P/ Cmd + Shift + P` and select 'Live Share: Start a collaboration session (Share)'
  - A unique invitation link will automatically be copied to your clipboard which can be shared with others who wish to join your session
  - To access the invitation link again, click on the session state status bar icon and select 'Invite Others (Copy Link)'
- Once you start your session, a pop-up message will notify you that your link has been copied to your clipboard and will allow you to select 'Make read-only' if you wish to prevent guests from editing your files
- If 'read-only' mode is not enabled, hosts and guests both have access to co-edit all files within the development environment as well as view each others edits in real-time
  - Co-editing abilities may be limited, dependending on `language and platform support <https://docs.microsoft.com/en-us/visualstudio/liveshare/reference/platform-support>`_
- You will be notified as guests join your session via your invitation link which will also grant you the option to remove them from the session 
- To terminate your session, open the 'Live Share' custom tab and select 'Stop collaboration session'
  - After the session has ended, guests will no longer have access to any content and all temp files will be cleaned up 

Troubleshooting
-----------------

On **Windows**, connecting GitHub to Visual Studio Code often has issues with GitHub Actions Permissions. VS Code often fails to request the "Workflow" permission, which is necessary for running the GitHub Actions we have on our repository. If you run into an error when pushing to a forked branch: `[remote rejected] <branch name> -> <branch name> (refusing to allow an OAuth App to create or update workflow`, it is likely because you don't have the "Workflow" scope on the OAuth link you accepted to connect VS Code and GitHub. 

To fix this, simply change the long OAuth link VS Code sends you to: :code:`scope=repo` should be :code:`scope=repo,workflow`. Once you update the link and load the page, you should be able to accept the updated permissions and push to GitHub.
