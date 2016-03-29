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

# To add new examples, append the directory name to this variable
examples = bridge basic_monitor simple_forward speed_tester
clean_examples=$(addprefix clean_,$(examples))

.PHONY: $(examples) $(clean_examples)

all : $(examples)
clean: $(clean_examples)

$(examples):
	cd $@ && $(MAKE)

$(clean_examples):
	cd $(patsubst clean_%,%,$@) && $(MAKE) clean

