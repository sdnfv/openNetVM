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
onvm_mgr_bin_coremask = ""
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
the coremasks for each openNetVM process: the manager and each NF.
"""
def onvm_coremask():
		global onvm_mgr_hex_coremask
		global onvm_mgr_bin_coremask

		num_mgr_thread = args.mgr		# If used, minimum requirement is 1 TX thread

		# Run calculations for openNetVM coremasks
		# ONVM Manager defaults to three threads 1x RX, 1x TX, 1x stat
		const_mgr_thread = 3;
		total_mgr_thread = const_mgr_thread + num_mgr_thread

		rem_cores = len(cores) - total_mgr_thread

		# Error checking
		if num_mgr_thread < 0:
			print "ERROR: You cannot run the manager with less than 0 TX threads for NFs"
			parser.print_help()
			raise SystemExit
		elif num_mgr_thread >= rem_cores:
			print "ERROR You cannot associate %d cores to the manager.  You will leave 0 cores to run the NFs" %(num_mgr_thread)
			parser.print_help()
			raise SystemExit

		# Based on required openNetVM manager cores, assign binary values
		onvm_mgr_bin_coremask = list("0" * len(cores))
		for i in range(len(cores)-1, len(cores)-1-total_mgr_thread, -1):
			onvm_mgr_bin_coremask[i] = "1"

		# Using remaining free cores in binary string, assign free core to
		#		NF.  One core can handle two NFs.
		nf_id = 0
		current_cpu = 0;

		while(rem_cores > 0):
			if rem_cores > 3:
				nf_core_mask = list("0" * len(cores))
				nf_core_mask2 = list("0" * len(cores))
				nf_core_mask[current_cpu] = "1"
				nf_core_mask2[(current_cpu+1)] = "1"

				onvm_nfs_coremasks[str(nf_id)] = (hex(int(''.join(nf_core_mask), 2)), nf_core_mask)
				onvm_nfs_coremasks[str((nf_id+1))] = (hex(int(''.join(nf_core_mask2), 2)), nf_core_mask2)
				onvm_mgr_bin_coremask[(rem_cores+nf_id)-1] = "1"

				current_cpu += 2
				nf_id += 2
				rem_cores -= 3
			elif rem_cores == 2:
				nf_core_mask = list("0" * len(cores))
				nf_core_mask[current_cpu] = "1"

				onvm_nfs_coremasks[str(nf_id)] = (hex(int(''.join(nf_core_mask), 2)), nf_core_mask)
				onvm_mgr_bin_coremask[(rem_cores+nf_id)-1] = "1"

				current_cpu += 1
				nf_id += 1
				rem_cores -= 2
			else:
				# We do not have enough cores if we're here.
				# Don't do anything...
				break

		# Turn manager binary string to hex
		onvm_mgr_hex_coremask = hex(int(''.join(onvm_mgr_bin_coremask), 2))

"""
Print out openNetVM coremask info
"""
def onvm_coremask_print():
		onvm_print_header()

		print "openNetVM requires at least three cores for the manager."
		print "After three cores, the manager can have more too.  Since"
		print "NFs need a thread for TX, the manager can have many dedicated"
		print "TX threads which would all need a dedicated core."
		print ""
		print "Each NF running on openNetVM needs its own core too.  One manager"
		print "TX thread can be used to handle two NFs, but any more becomes"
		print "inefficient."
		print ""

		if len(onvm_nfs_coremasks) <= 0:
			print "You cannot run openNetVM with %d threads for the manager." %(args.mgr)
			print "With this configuration, there are no cores left for the NFs."
			print ""

			if args.verbose:
				print "\t- openNetVM Manager -- coremask: %s" %(onvm_mgr_hex_coremask)
				print "\t\t+ %s" %(onvm_mgr_bin_coremask)
				print "\t\t\t+ Bit index represents core number"
				print "\t\t\t+ If 1, that core is used.  If 0, that core is not used."
				print "\t\t\t+ LSB is rightmost.  MGR should always use core 0...core n"
		else:
			print "Use the following information to run openNetVM on this system:"
			print ""
			print "\t- openNetVM Manager -- coremask: %s" %(onvm_mgr_hex_coremask)

			if args.verbose:
				print "\t\t+ %s" %(onvm_mgr_bin_coremask)
				print "\t\t\t+ Bit index represents core number"
				print "\t\t\t+ If 1, that core is used.  If 0, that core is not used."
				print "\t\t\t+ LSB is rightmost.  MGR should always use core 0...core n"

			print ""
			print "\t- CPU Layout permits %d NFs with these coremasks:" %(len(onvm_nfs_coremasks))

			for i in range(0, len(onvm_nfs_coremasks)):
				print "\t\t+ NF %d -- coremask: %s" %(i, onvm_nfs_coremasks[str(i)][0])

				if args.verbose:
					print "\t\t\t+ %s" %(onvm_nfs_coremasks[str(i)][1])
					print "\t\t\t\t+ Bit index represents core number"
					print "\t\t\t\t+ If 1, that core is used.  If 0, that core is not used."
					print "\t\t\t\t+ LSB is rightmost.  MGR should always use core n...core 0"

def onvm_print_header():
		print "==============================================================="
		print "\t\t openNetVM CPU Coremask Helper"
		print "==============================================================="
		print ""

"""
Function contains program execution sequence
"""
def run():
		if args.all:
				dpdk_cpu_info()
				onvm_coremask()
				dpdk_cpu_info_print()
				onvm_coremask_print()
		elif args.onvm:
				dpdk_cpu_info()
				onvm_coremask()
				onvm_coremask_print()
		elif args.cpu:
				dpdk_cpu_info()
				dpdk_cpu_info_print()
		else:
				parser.print_help()

### Set up arg parsing
parser = argparse.ArgumentParser(description='openNetVM coremask helper script')

parser.add_argument("-m", "--mgr",
		type=int, default=0,
		help="Specify number of TX threads for the manager to use. Defualts to 1.")
parser.add_argument("-o", "--onvm",
		action="store_true",
		help="Display openNetVM coremask information.")
parser.add_argument("-c", "--cpu",
		action="store_true",
		help="Display CPU architecture only.")
parser.add_argument("-a", "--all",
		action="store_true",
		help="Display all CPU information.")
parser.add_argument("-v", "--verbose",
		action="store_true",
		help="Verbose mode displays detailed coremask info.")

args = parser.parse_args()

# Function call to run program
run()
