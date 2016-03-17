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

ONVM_CONST_MGR_THRD = 3

sockets = []
cores = []
core_map = {}

onvm_mgr_corelist = []
onvm_nfs_corelist = []

### Code from Intel DPDK ###

"""
This function reads the /proc/cpuinfo file and determines the CPU
architecture which will be used to determine corelists for each
openNetVM NF and the manager.
From Intel DPDK tools/cpu_layout.py
"""
def dpdk_cpu_info():
	global core_map
	global cores
	global core_map

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
	global core_map
	global cores
	global core_map

	max_processor_len = len(str(len(cores) * len(sockets) * 2 - 1))
	max_core_map_len = max_processor_len * 2 + len('[, ]') + len('Socket ')
	max_core_id_len = len(str(max(cores)))

	print ""
	print "============================================================"
	print "Core and Socket Information (as reported by '/proc/cpuinfo')"
	print "============================================================\n"
	print "cores = ",cores
	print "sockets = ", sockets
	print ""
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

### End Intel DPDK Codeblock ###

"""
This function uses the information read from /proc/cpuinfo and determines
the corelists for each openNetVM process: the manager and each NF.
"""
def onvm_corelist():
	global core_map
	global cores
	global core_map
	global onvm_mgr_corelist
	global onvm_nfs_corelist

	mgr_core_index = 0
	nf_core_index = len(cores)-1
	rem_cores = len(cores)

	# Run calculations for openNetVM corelists
	# ONVM Manager defaults to three threads 1x RX, 1x TX, 1x stat
	total_mgr_thread = ONVM_CONST_MGR_THRD

	for i in range(0, total_mgr_thread):
		onvm_mgr_corelist.append(core_map[(0, mgr_core_index)])
		mgr_core_index += 1
		rem_cores -= 1

	while (rem_cores > 0):
		if rem_cores >= 3:
			onvm_mgr_corelist.append(core_map[(0, cores[mgr_core_index])])
			onvm_nfs_corelist.append(core_map[(0, cores[nf_core_index])])
			onvm_nfs_corelist.append(core_map[(0, cores[nf_core_index-1])])
			mgr_core_index += 1
			nf_core_index -= 2
			rem_cores -= 3
		elif rem_cores == 2:
			onvm_mgr_corelist.append(core_map[(0, cores[mgr_core_index])])
			onvm_nfs_corelist.append(core_map[(0, cores[nf_core_index])])
			mgr_core_index += 1
			nf_core_index -= 1
			rem_cores -= 2
		else:
			break

"""
Print out openNetVM corelist info
"""
def onvm_corelist_print():
	global onvm_mgr_corelist
	global onvm_nfs_corelist

	onvm_print_header()

	print "** MAKE SURE HYPERTHREADING IS DISABLED **"
	print ""
	print "openNetVM requires at least three cores for the manager."
	print "After the three cores, the manager needs one core for every two NFs'"
	print "TX thread"
	print ""
	print "Each NF running on openNetVM needs its own core too.  One manager"
	print "TX thread can be used to handle two NFs, but any more becomes"
	print "inefficient."
	print ""

	print "Use the following information to run openNetVM on this system:"
	print ""

	mgr_corelist=""
	for c in onvm_mgr_corelist:
		for i in c:
			mgr_corelist += "%s," %(i)
	print "\t- openNetVM Manager corelist: %s" %(mgr_corelist[:len(mgr_corelist)-1])

	print ""

	print "\t- openNetVM can handle %d NFs on this system" %(len(onvm_nfs_corelist))

	for i in range(0, len(onvm_nfs_corelist)):
		print "\t\t- NF %d:" %(i),
		for c in onvm_nfs_corelist[i]:
			print "%s" %(c)

def onvm_print_header():
	print "==============================================================="
	print "\t\t openNetVM CPU Corelist Helper"
	print "==============================================================="
	print ""

"""
Function contains program execution sequence
"""
def run():
	if args.all:
		dpdk_cpu_info()
		onvm_corelist()
		dpdk_cpu_info_print()
		onvm_corelist_print()
	elif args.onvm:
		dpdk_cpu_info()
		onvm_corelist()
		onvm_corelist_print()
	elif args.cpu:
		dpdk_cpu_info()
		dpdk_cpu_info_print()
	else:
		print "You supplied 0 arguments, running with flag --onvm"
		print ""

		dpdk_cpu_info()
		onvm_corelist()
		onvm_corelist_print()


if __name__ == "__main__":
	### Set up arg parsing
	parser = argparse.ArgumentParser(description='openNetVM corelist helper script')

	parser.add_argument("-o", "--onvm",
		action="store_true",
		help="[Default option] Display openNetVM corelist information.")
	parser.add_argument("-c", "--cpu",
		action="store_true",
		help="Display CPU architecture only.")
	parser.add_argument("-a", "--all",
		action="store_true",
		help="Display all CPU information.")
	parser.add_argument("-v", "--verbose",
		action="store_true",
		help="Verbose mode displays detailed corelist info.")

	args = parser.parse_args()

	# Function call to run program
	run()
