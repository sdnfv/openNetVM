/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2019 George Washington University
 *            2015-2019 University of California Riverside
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
 * onvm_config_common.c- contains config API functions and functions
 * to load an NF from a config file
 ********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "onvm_config_common.h"

#define IS_NULL_OR_EMPTY_STRING(s) ((s) == NULL || strncmp(s, "", 1) == 0 ? 1 : 0)

cJSON*
onvm_config_parse_file(const char* filename) {
        if (IS_NULL_OR_EMPTY_STRING(filename)) {
                return NULL;
        }

        FILE* fp = NULL;
        int file_length = 0;
        char temp_buf[255];
        char* tmp = NULL;
        char* line = NULL;
        char* json_str = NULL;
        int str_len = 0;

        fp = fopen(filename, "r");
        if (!fp) {
                return NULL;
        }

        fseek(fp, 0L, SEEK_END);
        file_length = ftell(fp);
        rewind(fp);

        json_str = (char*)malloc(file_length + 1);
        tmp = json_str;

        while ((line = fgets(temp_buf, file_length, fp)) != NULL) {
                str_len = (int)strlen(line);
                strncpy(tmp, line, str_len);
                tmp += str_len;
        }

        json_str[file_length] = '\0';
        fclose(fp);

        return cJSON_Parse(json_str);
}

int
onvm_config_extract_corelist(cJSON* dpdk_config, int* size, int** list) {
        cJSON* json_arr = NULL;
        int local_size = 0;
        int* local_list = NULL;
        int i = 0;

        if (dpdk_config == NULL || size == NULL || list == NULL) {
                return -1;
        }

        json_arr = cJSON_GetObjectItem(dpdk_config, "corelist");
        if (json_arr == NULL) {
                return -1;
        }

        local_size = cJSON_GetArraySize(json_arr);

        local_list = (int*)malloc(*size * sizeof(int));
        if (*list == NULL) {
                return -1;
        }

        for (i = 0; i < *size; ++i) {
                *list[i] = cJSON_GetArrayItem(json_arr, i)->valueint;
        }

        *size = local_size;
        *list = local_list;
        return 0;
}

int
onvm_config_extract_memory_channels(cJSON* dpdk_config, int* mem_channels) {
        struct cJSON* mem_channel_obj = NULL;

        if (dpdk_config == NULL || mem_channels == NULL) {
                return -1;
        }

        mem_channel_obj = cJSON_GetObjectItem(dpdk_config, "memory_channels");

        if (mem_channel_obj == NULL) {
                return -1;
        }

        *mem_channels = mem_channel_obj->valueint;

        return 0;
}

int
onvm_config_extract_portmask(cJSON* dpdk_config, int* portmask) {
        struct cJSON* portmask_obj = NULL;

        if (dpdk_config == NULL || portmask == NULL) {
                return -1;
        }

        portmask_obj = cJSON_GetObjectItem(dpdk_config, "portmask");

        if (portmask_obj == NULL) {
                return -1;
        }

        *portmask = portmask_obj->valueint;

        return 0;
}

int
onvm_config_extract_output_location(cJSON* onvm_config, char** output_loc) {
        char* local_output_loc = NULL;

        if (onvm_config == NULL || output_loc == NULL) {
                return -1;
        }

        if (cJSON_GetObjectItem(onvm_config, "output") == NULL) {
                return -1;
        }

        local_output_loc =
            (char*)malloc(sizeof(char) * strlen(cJSON_GetObjectItem(onvm_config, "output")->valuestring));
        if (local_output_loc == NULL) {
                return -1;
        }

        strncpy(local_output_loc, cJSON_GetObjectItem(onvm_config, "output")->valuestring,
                strlen(cJSON_GetObjectItem(onvm_config, "output")->valuestring));
        *output_loc = local_output_loc;

        return 0;
}

int
onvm_config_extract_service_id(cJSON* onvm_config, int* service_id) {
        if (onvm_config == NULL || service_id == NULL) {
                return -1;
        }

        if (cJSON_GetObjectItem(onvm_config, "serviceid") == NULL) {
                return -1;
        }

        *service_id = cJSON_GetObjectItem(onvm_config, "serviceid")->valueint;

        return 0;
}

int
onvm_config_extract_instance_id(cJSON* onvm_config, int* instance_id) {
        if (onvm_config == NULL || instance_id == NULL) {
                return -1;
        }

        if (cJSON_GetObjectItem(onvm_config, "instanceid") == NULL) {
                return -1;
        }

        *instance_id = cJSON_GetObjectItem(onvm_config, "instanceid")->valueint;

        return 0;
}

int
onvm_config_get_item_count(cJSON* config) {
        int arg_count = 0;

        if (config == NULL) {
                return 0;
        }

        if (config->child == NULL) {
                return 0;
        }

        cJSON* current = config->child;
        while (current != NULL) {
                ++arg_count;
                current = current->next;
        }

        return arg_count;
}

int
onvm_config_create_nf_arg_list(cJSON* config, int* argc, char** argv[]) {
        cJSON* dpdk_config = NULL;
        cJSON* onvm_config = NULL;
        char** dpdk_argv = NULL;
        char** onvm_argv = NULL;
        int dpdk_argc = -1;
        int onvm_argc = -1;
        char** new_argv = NULL;
        int new_argc = 0;
        int offset = 0;
        int onvm_new_argv_index = 0;
        int onvm_old_argv_index = 0;
        int nf_new_argv_index = 0;
        int nf_old_argv_index = 0;
        int i = 0;
        int j = 0;

        if (config == NULL) {
                return -1;
        }

        dpdk_config = cJSON_GetObjectItem(config, "dpdk");
        if (dpdk_config == NULL) {
                printf("Unable to find DPDK-specific config\n");
                return -1;
        }

        onvm_config = cJSON_GetObjectItem(config, "onvm");
        if (onvm_config == NULL) {
                printf("Unable to find ONVM-specific config\n");
                return -1;
        }

        if (onvm_config_create_dpdk_args(dpdk_config, &dpdk_argc, &dpdk_argv) < 0) {
                printf("Unable to generate DPDK arg list\n");
                return -1;
        }

        if (onvm_config_create_onvm_args(onvm_config, &onvm_argc, &onvm_argv) < 0) {
                printf("Unable to generate ONVM arg list\n");
                return -1;
        }

        /* Add all the argcs toggether, accounting for the program name */
        new_argc = 1 + dpdk_argc + onvm_argc;

        new_argv = (char**)malloc(sizeof(char*) * new_argc);
        if (new_argv == NULL) {
                printf("Unable to allocate space for new argv\n");
                return -1;
        }

        new_argv[0] = (char*)malloc(strlen((*argv)[0]));
        if (new_argv[0] == NULL) {
                return -1;
        }

        /* Copy the program name */
        strncpy(new_argv[0], (*argv)[0], strlen((*argv)[0]));

        /*First arg has been filled in, so offset by that */
        offset = 1;

        /* Copy the DPDK args to new_argv */
        for (i = 0; i < dpdk_argc; ++i) {
                new_argv[i + offset] = dpdk_argv[i];
        }

        /* DPDK args have been filled in, so offset by that */
        offset += dpdk_argc;

        /* Copy ONVM args to new_argv- need to use the offset so that the loop still starts at 0 */
        for (i = 0; i < onvm_argc; ++i) {
                new_argv[i + offset] = onvm_argv[i];
        }

        /* Find the dashes in the argv from the original command-line entry */
        for (i = 0; i < *argc; ++i) {
                if (strncmp((*argv)[i], FLAG_DASH, strlen(FLAG_DASH)) == 0) {
                        if (onvm_old_argv_index == 0) {
                                onvm_old_argv_index = i + 1;
                        } else {
                                nf_old_argv_index = i + 1;
                        }
                }
        }

        /* Find the dashes in the newly constructed argv */
        for (i = 0; i < new_argc; ++i) {
                if (strncmp(new_argv[i], FLAG_DASH, strlen(FLAG_DASH)) == 0 &&
                    strlen(new_argv[i]) == strlen(FLAG_DASH)) {
                        if (onvm_new_argv_index == 0) {
                                onvm_new_argv_index = i + 1;
                        } else {
                                nf_new_argv_index = i + 1;
                        }
                }
        }

        /* If no NF-specific arguments were passed in, the second dash can be said to the end of argv */
        if (nf_old_argv_index == 0) {
                nf_old_argv_index = *argc;
        }

        if (nf_new_argv_index == 0) {
                nf_new_argv_index = new_argc;
        }

        /* Check the old argv for DPDK values and memcpy the old value to the new_argv array */
        for (i = 1; i < onvm_new_argv_index - 1; ++i) {
                for (j = 3; j < onvm_old_argv_index - 1; ++j) {
                        if (strncmp((*argv)[j], new_argv[i], strlen(new_argv[i])) == 0 && j % 2 == 1) {
                                memcpy(new_argv[i + 1], (*argv)[j + 1], strlen((*argv)[j + 1]));
                        }
                }
        }

        /* Check the old argv for ONVM values and memcpy the old value to the new_argv array */
        for (i = onvm_new_argv_index; i < nf_new_argv_index; ++i) {
                for (j = onvm_old_argv_index; j < nf_old_argv_index; ++j) {
                        if (strncmp((*argv)[j], new_argv[i], strlen(new_argv[i])) == 0 && j % 2 == 0) {
                                memcpy(new_argv[i + 1], (*argv)[j + 1], strlen((*argv)[j + 1]));
                        }
                }
        }

        /* Need to clear out the old args so that the NF won't double count them- it should only see NF-specific args */
        for (i = 0; i < nf_old_argv_index; ++i) {
                memset((*argv)[i], '\0', strlen((*argv)[i]));
        }

        *argv = new_argv;
        *argc = new_argc;

        return 0;
}

int
onvm_config_create_onvm_args(cJSON* onvm_config, int* onvm_argc, char** onvm_argv[]) {
        char* service_id_string = NULL;
        char* instance_id_string = NULL;
        int service_id = 0;
        int instance_id = 0;

        /* An NF has 2 required ONVM args */
        *onvm_argc = 2;

        if (onvm_config == NULL || onvm_argc == NULL || onvm_argv == NULL) {
                return -1;
        }

        if (onvm_config_extract_service_id(onvm_config, &service_id)) {
                printf("Unable to extract service ID\n");
                return -1;
        }

        if (onvm_config_extract_instance_id(onvm_config, &instance_id) > -1) {
                /* Need to account for instance id args, so add 2 */
                *onvm_argc += 2;
        }

        *onvm_argv = (char**)malloc(sizeof(char*) * (*onvm_argc));
        if (*onvm_argv == NULL) {
                printf("Unable to allocate space for onvm_argv\n");
                return -1;
        }

        /* Allows for MAX_SERVICE_ID_SIZE number of characters in the number */
        service_id_string = (char*)malloc(sizeof(char) * MAX_SERVICE_ID_SIZE);
        if (service_id_string == NULL) {
                printf("Unable to allocate space for onvm_service_id_string\n");
                return -1;
        }

        (*onvm_argv)[0] = malloc(sizeof(char) * strlen(FLAG_R));
        if ((*onvm_argv)[0] == NULL) {
                printf("Unable to allocate space for onvm_argv[0]\n");
                return -1;
        }

        strncpy((*onvm_argv)[0], FLAG_R, strlen(FLAG_R));

        snprintf(service_id_string, sizeof(char) * MAX_SERVICE_ID_SIZE, "%d", service_id);
        (*onvm_argv)[1] = service_id_string;

        if (*onvm_argc > 2) {
                instance_id_string = (char*)malloc(sizeof(char) * MAX_SERVICE_ID_SIZE);
                if (instance_id_string == NULL) {
                        printf("Unable to allocate space for onvm_instance_id_string\n");
                        return -1;
                }

                (*onvm_argv)[2] = malloc(sizeof(char) * strlen(FLAG_N));
                if ((*onvm_argv)[2] == NULL) {
                        printf("Could not allocate space for instance id in argv\n");
                        return -1;
                }
                strncpy((*onvm_argv)[2], FLAG_N, strlen(FLAG_N));
                snprintf(instance_id_string, sizeof(char) * MAX_SERVICE_ID_SIZE, "%d", instance_id);
                (*onvm_argv)[3] = instance_id_string;
        }

        return 0;
}

int
onvm_config_create_dpdk_args(cJSON* dpdk_config, int* dpdk_argc, char** dpdk_argv[]) {
        char* core_string = NULL;
        int mem_channels = 0;
        char* mem_channels_string = NULL;
        size_t* arg_size = NULL;
        size_t mem_channels_string_size;
        int i = 0;

        if (dpdk_config == NULL || dpdk_argc == NULL || dpdk_argv == NULL) {
                printf("DPDK ARGV: %p\n", dpdk_config);
                printf("An arg was null\n");
                return -1;
        }

        /* DPKD requires 6 args */
        *dpdk_argc = 6;

        *dpdk_argv = (char**)malloc(sizeof(char*) * (*dpdk_argc));
        if (*dpdk_argv == NULL) {
                printf("Unable to allocate space for dpdk_argv\n");
                return -1;
        }

        arg_size = (size_t*)malloc(sizeof(size_t) * (*dpdk_argc));
        if (arg_size == NULL) {
                return -1;
        }

        core_string = (char*)malloc(sizeof(char) * strlen(cJSON_GetObjectItem(dpdk_config, "corelist")->valuestring));
        if (core_string == NULL) {
                printf("Unable to allocate space for core string\n");
                return -1;
        }

        core_string = strncpy(core_string, cJSON_GetObjectItem(dpdk_config, "corelist")->valuestring,
                              strlen(cJSON_GetObjectItem(dpdk_config, "corelist")->valuestring));

        if (onvm_config_extract_memory_channels(dpdk_config, &mem_channels) < 0) {
                printf("Unable to extract memory channels\n");
                return -1;
        }

        mem_channels_string_size = sizeof(char) * 3;
        mem_channels_string = (char*)malloc(mem_channels_string_size);
        if (mem_channels_string == NULL) {
                printf("Unable to allocate space for memory channels string\n");
                return -1;
        }

        snprintf(mem_channels_string, mem_channels_string_size, "%d", mem_channels);

        arg_size[0] = strlen(FLAG_L);
        arg_size[1] = strlen(core_string);
        arg_size[2] = strlen(FLAG_N);
        arg_size[3] = strlen(mem_channels_string);
        arg_size[4] = strlen(PROC_TYPE_SECONDARY);
        arg_size[5] = strlen(FLAG_DASH);

        for (i = 0; i < *dpdk_argc; ++i) {
                (*dpdk_argv)[i] = (char*)malloc(arg_size[i]);

                if ((*dpdk_argv)[i] == NULL) {
                        return -1;
                }
        }

        strncpy((*dpdk_argv)[0], FLAG_L, arg_size[0]);
        strncpy((*dpdk_argv)[1], core_string, arg_size[1]);
        strncpy((*dpdk_argv)[2], FLAG_N, arg_size[2]);
        strncpy((*dpdk_argv)[3], mem_channels_string, arg_size[3]);
        strncpy((*dpdk_argv)[4], PROC_TYPE_SECONDARY, arg_size[4]);
        strncpy((*dpdk_argv)[5], FLAG_DASH, arg_size[5]);

        free(arg_size);
        free(core_string);
        free(mem_channels_string);

        return 0;
}
