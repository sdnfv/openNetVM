#! /usr/bin/python

#                    openNetVM
#      https://github.com/sdnfv/openNetVM
#
# BSD LICENSE
#
# Copyright(c)
#          2015-2018 George Washington University
#          2015-2018 University of California Riverside
#          2010-2014 Intel Corporation.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in
# the documentation and/or other materials provided with the
# distribution.
# The name of the author may not be used to endorse or promote
# products derived from this software without specific prior
# written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import sys
import argparse
import os

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

	core_index = 0
	total_cores = len(cores)

	# Run calculations for openNetVM corelists
	# ONVM Manager defaults to three threads 1x RX, 1x TX, 1x stat

	while core_index < ONVM_CONST_MGR_THRD:
		core = core_map.get((0, core_index), None)
		if core is None:
			print "Not enough cores: onvm requires {} to run manager. (You have {})".format(ONVM_CONST_MGR_THRD, len(core_map))
			exit(1)
		onvm_mgr_corelist.append(core)
		core_index += 1

	while core_index < total_cores:
		onvm_nfs_corelist.append(core_map[(0, cores[core_index])])
		core_index += 1


"""
Reads the output of lscpu to determine if hyperthreading is enabled.
"""
def onvm_ht_isEnabled():
	lscpu_output = os.popen('lscpu -p').readlines()
	for line in lscpu_output:
		try:
			line_csv = line.split(',')
			phys_core = int(line_csv[0])
			logical_core = int(line_csv[1])
			if phys_core != logical_core:
				return True
		except ValueError:
			pass

	return False

"""
Print out openNetVM corelist info
"""
def onvm_corelist_print():
	global onvm_mgr_corelist
	global onvm_nfs_corelist

	onvm_print_header()

	if onvm_ht_isEnabled():
		print "This script only works if hyperthreading is disabled."
		print "Run no_hyperthread.sh to disable hyperthreading before "
		print "running this script again."
		print ""
		exit(1)

	print "** MAKE SURE HYPERTHREADING IS DISABLED **"
	print ""
	print "openNetVM requires at least three cores for the manager:"
	print "one for NIC RX, one for statistics, and one for NIC TX."
 	print "For rates beyond 10Gbps it may be necessary to run multiple TX"
	print "or RX threads."
	print ""
	print "Each NF running on openNetVM needs its own core too."
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

	for i, cores in enumerate(onvm_nfs_corelist, 1):
		print "\t\t- NF %d:" %(i),
		for c in cores:
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
