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

RTE_TARGET ?= x86_64-native-linuxapp-gcc

# Default target, can be overriden by command line or environment
include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = flow_table

# all source are stored in SRCS-y
SRCS-y := flow_table.c sdn.c setupconn.c msgbuf.c

# OpenNetVM path
ONVM ?= $(SRCDIR)/../../onvm

#CFLAGS += $(WERROR_FLAGS) -O3 $(USER_FLAGS)
CFLAGS += $(WERROR_FLAGS) -g -O0 $(USER_FLAGS)

CFLAGS += -I$(ONVM)/onvm_nf
CFLAGS += -I$(ONVM)/shared
LDFLAGS += $(ONVM)/onvm_nf/onvm_nf/x86_64-native-linuxapp-gcc/onvm_nflib.o
LDFLAGS += $(ONVM)/shared/shared/x86_64-native-linuxapp-gcc/onvm_pkt_helper.o
LDFLAGS += $(ONVM)/shared/shared/x86_64-native-linuxapp-gcc/onvm_sc_common.o
LDFLAGS += $(ONVM)/shared/shared/x86_64-native-linuxapp-gcc/onvm_flow_table.o
LDFLAGS += $(ONVM)/shared/shared/x86_64-native-linuxapp-gcc/onvm_flow_dir.o

# workaround for a gcc bug with noreturn attribute
# http://gcc.gnu.org/bugzilla/show_bug.cgi?id=12603
ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
CFLAGS_main.o += -Wno-return-type
endif

include $(RTE_SDK)/mk/rte.extapp.mk
