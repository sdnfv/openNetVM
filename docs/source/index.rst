.. OpenNetVM documentation master file, created by
   sphinx-quickstart on Thu Jul 23 19:12:38 2020.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Welcome to OpenNetVM!
=====================================

OpenNetVM is a high performance NFV platform based on DPDK and Docker containers. OpenNetVM provides a flexible framework for deploying network functions and interconnecting them to build service chains.

OpenNetVM is an open source version of the NetVM platform described in our NSDI 2014 and HotMiddlebox 2016 papers, released under the BSD license.

The develop branch tracks experimental builds (active development) whereas the master branch tracks verified stable releases. Please read our releases document for more information about our releases and release cycle.

You can find information about research projects building on OpenNetVM at the UCR/GW SDNFV project site. OpenNetVM is supported in part by NSF grants CNS-1422362 and CNS-1522546.

Notes
####################################

We have updated our DPDK submodule to point to a new version, v18.11. If you have already cloned this repository, please update your DPDK submodule by running:

.. code-block:: bash
   :linenos:

   git submodule sync
   git submodule update --init

And then rebuild DPDK using the install guide or running these commands:

.. code-block:: bash
   :linenos:

   cd dpdk
   make config T=$RTE_TARGET
   make T=$RTE_TARGET -j 8
   make install T=$RTE_TARGET -j 8

See our release document for more information.

About
###################

OpenNetVM is a high performance NFV platform based on `DPDK <http://dpdk.org>`_ and `Docker <https://www.docker.com/>`_ containers.  OpenNetVM provides a flexible framework for deploying network functions and interconnecting them to build service chains.

OpenNetVM is an open source version of the NetVM platform described in our `NSDI 2014 <http://faculty.cs.gwu.edu/timwood/papers/14-NSDI-netvm.pdf>`_ and `HotMiddlebox 2016 <http://faculty.cs.gwu.edu/timwood/papers/16-HotMiddlebox-onvm.pdf>`_ papers, released under the `BSD <https://github.com/sdnfv/openNetVM/blob/master/LICENSE>`_ license.  

The `develop <https://github.com/sdnfv/OpenNetVM/tree/develop>`_ branch tracks experimental builds (active development) whereas the `master <https://github.com/sdnfv/OpenNetVM/tree/master>`_ branch tracks verified stable releases.  Please read our `releases </releases>`_ document for more information about our releases and release cycle.

You can find information about research projects building on `OpenNetVM <http://sdnfv.github.io/onvm/>`_ at the `UCR/GW SDNFV project site <http://sdnfv.github.io/>`_. OpenNetVM is supported in part by NSF grants CNS-1422362 and CNS-1522546. 

Installing
######################

To install OpenNetVM, please see the `OpenNetVM Installation <install>`_ guide for a thorough walkthrough.

Using OpenNetVM
#######################

OpenNetVM comes with several sample network functions.  To get started with some examples, please see the `Example Uses <examples>`_ guide

Creating NFs
########################

The `NF Development <nfdev>`_ guide will provide what you need to start creating your own NFs.

Dockerize NFs
#######################

NFs can be run inside docker containers, with the NF being automatically or hand started. For more informations, see our `Docker guide <docker>`_.

TCP Stack
#######################

OpenNetVM can run mTCP applications as NFs. For more information, visit `mTCP <https://github.com/eunyoung14/mtcp>`_.

Citing OpenNetVM
#######################

If you use OpenNetVM in your work, please cite our paper:

.. code-block:: bash
   :linenos:

   @inproceedings{zhang_opennetvm:_2016,
      title = {{OpenNetVM}: {A} {Platform} for {High} {Performance} {Network} {Service} {Chains}},
      booktitle = {Proceedings of the 2016 {ACM} {SIGCOMM} {Workshop} on {Hot} {Topics} in {Middleboxes} and {Network} {Function} {Virtualization}},
      publisher = {ACM},
      author = {Zhang, Wei and Liu, Guyue and Zhang, Wenhui and Shah, Neel and Lopreiato, Phillip and Todeschi, Gregoire and Ramakrishnan, K.K. and Wood, Timothy},
      month = aug,
      year = {2016},
   }

Please let us know if you use OpenNetVM in your research by `emailing us <mailto:timwood@gwu.edu>`_ or completing this `short survey <https://goo.gl/forms/oxcnGO45Kxq1Zyyi2>`_.

.. toctree::
   :caption: Getting Started
   :hidden:
   :maxdepth: -1
   
   Installation Guide </install/index>
   Development Environment <devenv/index>
   Debugging </debug/index>

.. toctree::
   :caption: Step-By-Step Guides
   :hidden:
   :maxdepth: -1

   Cloudlab Tutorial <cloudlabtutorial/index>
   Three Node Topology Tutorial <threenodetutorial/index>
   Packet Generation Using Pktgen <pktgen/index>
   Running OpenNetVM in Docker <docker/index>
   MoonGen Installation (DPDK-2.0 Version) <moongen/index>
   Wireshark & Pdump for Packet Sniffing <wireshark/index>

.. toctree::
   :caption: Advanced Features
   :hidden:
   :maxdepth: -1

   OpenNetVM Examples <examples/index>
   NF API <nfapi/index>
   NF Development <nfdev/index>

.. toctree::
   :caption: About OpenNetVM
   :hidden:
   :maxdepth: -1

   Contributing </contributing/index>
   Release Notes <releases/index>
   Frequently Asked Questions <faq/index>
   CI Development <cidev/index>
