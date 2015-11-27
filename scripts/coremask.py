#! /usr/bin/python

#                    openNetVM
#      https://github.com/sdnfv/openNetVM
#
# Copyright 2015 George Washington University
#           2015 University of California Riverside
#           2010-2014 Intel Corporation. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import sys
import argparse

sockets = []
cores = []
core_map = {}

onvm_mgr_hex_coremask = ""
onvm_nfs_coremasks = {}

### Code from Intel DPDK ###

"""
This function reads the /proc/cpuinfo file and determines the CPU
architecture which will be used to determine coremasks for each
openNetVM NF and the manager.
From Intel DPDK tools/cpu_layout.py
"""
def dpdk_cpu_info():
		fd=open("/proc/cpuinfo")
		lines = fd.readlines()
		fd.close()

		core_details = []
		core_lines = {}
		for line in lines:
			if len(line.strip()) != 0:
				name, value = line.split(":", 1)
				core_lines[name.strip()] = value.strip()
			else:
				core_details.append(core_lines)
				core_lines = {}

		for core in core_details:
			for field in ["processor", "core id", "physical id"]:
				if field not in core:
					print "Error getting '%s' value from /proc/cpuinfo" % field
					sys.exit(1)
				core[field] = int(core[field])

			if core["core id"] not in cores:
				cores.append(core["core id"])
			if core["physical id"] not in sockets:
				sockets.append(core["physical id"])
			key = (core["physical id"], core["core id"])
			if key not in core_map:
				core_map[key] = []
			core_map[key].append(core["processor"])

"""
Print out CPU architecture info.
From Intel DPDK tools/cpu_layout.py
"""
def dpdk_cpu_info_print():
		max_processor_len = len(str(len(cores) * len(sockets) * 2 - 1))
		max_core_map_len = max_processor_len * 2 + len('[, ]') + len('Socket ')
		max_core_id_len = len(str(max(cores)))

		print " ".ljust(max_core_id_len + len('Core ')),
		for s in sockets:
		        print "Socket %s" % str(s).ljust(max_core_map_len - len('Socket ')),
		print ""
		print " ".ljust(max_core_id_len + len('Core ')),
		for s in sockets:
		        print "--------".ljust(max_core_map_len),
		print ""

		for c in cores:
		        print "Core %s" % str(c).ljust(max_core_id_len),
		        for s in sockets:
		                print str(core_map[(s,c)]).ljust(max_core_map_len),
		        print "\n"

		print "============================================================"
		print "Core and Socket Information (as reported by '/proc/cpuinfo')"
		print "============================================================\n"
		print "cores = ",cores
		print "sockets = ", sockets
		print ""

### End Intel DPDK Codeblock ###

### Additions by @nks5295 ###

"""
This function uses the information read from /proc/cpuinfo and determines
the coremasks for each openNetVM process: the manager and each NF.
"""
def onvm_coremask():
		global onvm_mgr_hex_coremask
		# TODO: Assume 1 tx thread for mgr.  change to input based

		# Run calculations for openNetVM coremasks
		mgr_binary_core_mask = list("0" * len(cores))

		const_mgr_thread = 3;	# 1x rx, 1x tx, 1x stat
		# TODO: fix this one
		num_mgr_thread = 1;		# threads for each nf tx
		total_mgr_thread = const_mgr_thread + num_mgr_thread

		# Based on required openNetVM manager cores, assign binary values
		for i in range(len(cores)-1, len(cores)-1-total_mgr_thread, -1):
			mgr_binary_core_mask[i] = "1"

		onvm_mgr_hex_coremask = hex(int(''.join(mgr_binary_core_mask), 2))

		# Using remaining free cores in binary string, assign free core to
		# NF.  One core can handle two NFs.  Right now, there is a 1:1 NF:Core
		# mapping.
		nf_id = 0
		for i in range(0, len(cores)-total_mgr_thread, 1):
			nf_core_mask = list("0" * len(cores))
			nf_core_mask[i] = "1"

			onvm_nfs_coremasks[str(nf_id)] = hex(int(''.join(nf_core_mask), 2))

			nf_id += 1

"""
Print out openNetVM coremask info
"""
def onvm_coremask_print():
		print "==============================================================="
		print "\t\t openNetVM CPU Coremask Helper"
		print "==============================================================="
		print ""

		print "Use this information to run openNetVM:"
		print ""

		print "\t- openNetVM Manager -- coremask: %s" %(onvm_mgr_hex_coremask)
		print ""
		print "\t- CPU Layout permits %d NFs with these coremasks:" %(len(onvm_nfs_coremasks))

		for i in range(0, len(onvm_nfs_coremasks)):
			print "\t\t+ NF %d -- coremask: %s" %(i, onvm_nfs_coremasks[str(i)])

### End additions by @nks5295 ###

dpdk_cpu_info()
onvm_coremask()
onvm_coremask_print()
