.. openNetVM documentation master file, created by
   sphinx-quickstart on Thu Jul 23 19:12:38 2020.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Welcome to openNetVM!
=====================================

openNetVM is a high performance NFV platform based on DPDK and Docker containers. openNetVM provides a flexible framework for deploying network functions and interconnecting them to build service chains.

openNetVM is an open source version of the NetVM platform described in our NSDI 2014 and HotMiddlebox 2016 papers, released under the BSD license.

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

.. toctree::
   :caption: Contents
   :hidden:
   :maxdepth: 2

   Installation Guide </install/index>
   Contributing </contributing/index>
   Debugging </debug/index>
   Running OpenNetVM in Docker <docker/index>
   OpenNetVM Examples <examples/index>
   NF Development <nfdev/index>
   Release Notes <releases/index>
   MoonGen Installation (DPDK-2.0 Version) <moongen/index>
   CI Development <cidev/index>
   Development Environment <devenv/index>
   NF API <nfapi/index.rst>
   Frequently Asked Questions <faq/index>
   Packet Generation Using Pktgen <pktgen/index>
   Wireshark & Pdump for Packet Sniffing <wireshark/index>
   Three Node Topology Tutorial <threenodetutorial/index>