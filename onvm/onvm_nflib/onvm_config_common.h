/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2019 George Washington University
 *            2015-2019 University of California Riverside
 *            2016-2019 Hewlett Packard Enterprise Development LP
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * The name of the author may not be used to endorse or promote
 *       products derived from this software without specific prior
 *       written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ********************************************************************/

#ifndef _ONVM_CONFIG_COMMON_H_
#define _ONVM_CONFIG_COMMON_H_

#include "cJSON.h"

#define MAX_SERVICE_ID_SIZE 5

/***********************Command Line Arg Strings**********************/

#define PROC_TYPE_SECONDARY "--proc-type=secondary"
#define FLAG_N "-n"
#define FLAG_R "-r"
#define FLAG_L "-l"
#define FLAG_DASH "--"

/*****************************API************************************/

/**
 * Parses a config file into a cJSON struct.
 * This struct contains all information stored within the config file
 *
 * @param filename
 *   A pointer to the name of the config file
 * @return
 *   Returns a pointer to a cJSON struct containing config information
 */
cJSON*
onvm_config_parse_file(const char* filename);

/**
 * Extracts the corelist from a config file.
 *
 * @param dpdk_config
 *   Pointer to cJSON struct with the parsed DPDK config file
 * @param size
 *   A pointer to hold number of cores specified
 * @param list
 *   Pointer to hold the array of cores
 * @return
 *   0 on success, and -1 if any parameters are NULL or the function fails
 */
int
onvm_config_extract_corelist(cJSON* dpdk_config, int* size, int** list);

/*
 * Extracts the memory channels for DPDK.
 *
 * @param dpdk_config
 *   Pointer to a cJSON struct containing parsed DPDK file.
 * @param mem_channels
 *   Pointer to hold extracted memory channels
 * @return
 *   0 on success, and -1 if failure
 */
int
onvm_config_extract_memory_channels(cJSON* dpdk_config, int* mem_channels);

/**
 * Extracts the portmask from a config file.
 * This is used when launching the manager.
 *
 * @param config
 *   Pointer to cJSON struct containing parsed DPDK config file
 * @param portmask
 *   Pointer to hold the extracted portmask
 * @return
 *   Returns 0 on success, or -1 if failure
 */
int
onvm_config_extract_portmask(cJSON* onvm_config, int* portmask);

/**
 * Extracts the location of output (stdout, web, etc.).
 * This is mostly used when launching the manager.
 *
 * @param onvm_config
 *   Pointer to cJSON struct with the parsed onvm config file
 * @param output_loc
 *   Pointer to hold the string with the specified output location
 * @return
 *   0 on success, -1 if failure
 */
int
onvm_config_extract_output_location(cJSON* onvm_config, char** output_loc);

/**
 * Extracts the service ID for an NF.
 *
 * @param onvm_config
 *   Pointer to a cJSON struct with the parsed onvm config file
 * @param service_id
 *   Pointer to hold extracted service id
 * @return
 *   0 on success, -1 if failure
 */
int
onvm_config_extract_service_id(cJSON* onvm_config, int* service_id);

/**
 * Extracts the instance ID for an NF. Replaces the -n for ONVM settings
 *
 * @param onvm_config
 *   Pointer to a cJSON struct with the parsed onvm config file
 * @param instance_id
 *   Pointer to hold the extracted instance id
 * @return
 *   0 on success, -1 if failure
 */
int
onvm_config_extract_instance_id(cJSON* onvm_config, int* instance_id);

/*
 * Gets the number of items in a JSON section
 *
 * @param config
 *   Pointer to a cJSON struct
 * @return
 *   Returns the number of items in the config struct
 */
int
onvm_config_get_item_count(cJSON* config);

/*
 * Creates new argc and argv parameters from the values inside the config file
 *
 * @param config
 *   Pointer to cJSON struct with the NF parameters
 * @param argc
 *   Pointer to the argc variable from NFLib init
 * @param argv
 *   Pointer to the argv variable that stores the string args
 * @return
 *   Returns a 0 on success, and -1 otherwise
 */
int
onvm_config_create_nf_arg_list(cJSON* config, int* argc, char** argv[]);

/*
 * Creates the arg list for ONVM settings
 *
 * @param onvm_config
 *   Pointer to a cJSON struct that contains all of the ONVM config settings
 * @param onvm_argc
 *   Pointer to the counter that holds how many ONVM args there are
 * @param onvm_argv
 *   Pointer to the array of string args related to ONVM
 * @return
 *   0 if onvm_argv is succesfully populated, -1 otherwise
 */
int
onvm_config_create_onvm_args(cJSON* onvm_config, int* onvm_argc, char** onvm_argv[]);

/*
 * Creates the arg list for DPDK settings
 *
 * @param dpdk_config
 *   Pointer to a cJSON struct that holds DPDK config settings
 * @param dpdk_argc
 *   Pointer to the counter that stores how many DPDK arguments are stored in dpdk_argv
 * @param dpdk_argv
 *   Pointer to the array of strings that holds DPDK arguments
 * @return
 *   0 if dpdk_argv is succesfully populated, -1 otherwise
 */
int
onvm_config_create_dpdk_args(cJSON* dpdk_config, int* dpdk_argc, char** dpdk_argv[]);

#endif  // _ONVM_CONFIG_COMMON_H_
