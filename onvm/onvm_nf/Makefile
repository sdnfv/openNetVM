# /*********************************************************************
#  *                     openNetVM
#  *       https://github.com/sdnfv/openNetVM
#  *
#  *  Copyright 2015 George Washington University
#  *            2015 University of California Riverside
#  *
#  *  Licensed under the Apache License, Version 2.0 (the "License");
#  *  you may not use this file except in compliance with the License.
#  *  You may obtain a copy of the License at
#  *
#  *      http://www.apache.org/licenses/LICENSE-2.0
#  *
#  *  Unless required by applicable law or agreed to in writing, software
#  *  distributed under the License is distributed on an "AS IS" BASIS,
#  *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  *  See the License for the specific language governing permissions and
#  *  limitations under the License.
#  *
#  ********************************************************************/

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overriden by command line or environment
include $(RTE_SDK)/mk/rte.vars.mk

RTE_TARGET ?= x86_64-native-linuxapp-gcc

# all source are stored in SRCS-y
SRCS-y := onvm_nflib.c

CFLAGS += $(WERROR_FLAGS) -O3 $(USER_FLAGS)
CFLAGS += -I$(SRCDIR)/../shared

LDFLAGS += $(SRCDIR)/../shared/shared/x86_64-native-linuxapp-gcc/onvm_pkt_helper.o

include $(RTE_SDK)/mk/rte.extobj.mk

