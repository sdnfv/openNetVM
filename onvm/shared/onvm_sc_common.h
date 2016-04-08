/*********************************************************************
 *                     openNetVM
 *       https://github.com/sdnfv/openNetVM
 *
 *  Copyright 2015 George Washington University
 *            2015 University of California Riverside
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 * onvm_sc_common.h - service functions for manager and NFs
 ********************************************************************/

#ifndef _SC_COMMON_H_
#define _SC_COMMON_H_

#include <inttypes.h>
#include "common.h"

/* append a entry to serivce chain, 0 means appending successful, 1 means failed*/
int onvm_sc_append_entry(struct onvm_service_chain *chain, uint8_t action, uint16_t destination);

/*set entry to a new action and destination, 0 means setting successful, 1 means failed */
int onvm_sc_set_entry(struct onvm_service_chain *chain, int entry, uint8_t action, uint16_t destination);

void onvm_sc_print(struct onvm_service_chain *chain);
#endif //_SC_COMMON_H_
