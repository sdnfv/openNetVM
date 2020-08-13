CI Development
=====================================

This page will serve as description and development plans for our CI system

**Current CI maintainers: primary - @kevindweb, secondary - @koolzz**


CI is being moved to a new repository inside :code:`sdnfv` called called:`onvm-ci` you can find it `here <https://github.com/sdnfv/onvm-ci)>`_.

Currently CI does 3 main functions:
-------------------------------------

1. Checks if the PR was submitted to the :code:`develop` branch

2. Runs Linter and reports errors (only on lines modified by the diff)

3. Runs the Speed Tester NF and Pktgen and reports measured performance

Done
------

1. Event queue ✔️ 
    - When the CI is busy with another request it will deny all other requests and respond with a *I'm busy, retry in 10 min* message, which is clearly not ideal for a good CI system. CI should implement some type of request queue to resolve this.

2. Pktgen Tests in addition to Speed Tester
    - Pktgen testing, with a 2 node setup where one node would be running Pktgen and the other would run the Basic Monitor NF. This is done both by reserving 2 worker nodes that are directly connected.

Planned features
------------------

1. Expanding performance test coverage
    - We should also include mTCP test with epserver and epwget
    - Additionally, we need a X node chain speed tester performance

2. Linter coverage
    - Currently we only lint .c, .h, .cpp files. We have a lot of python/shell scripts that we should also be linting.

3. Github Messaging
    - With the new CI modes and a message editing api from github3.py, we are able to post responses as they come in. 
    - Linting data comes much faster than speed testing for example, so NF data will be appended to an already posted message in Github for quick responses.

4. Better resource management(long term)
    - Utilizing the new cluster software we can figure out which nodes are unused and always have them ready to run CI tests. This would help speed up the CI process. 
