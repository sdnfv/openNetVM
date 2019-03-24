--                    openNetVM
--      https://github.com/sdnfv/openNetVM
--
-- BSD LICENSE
--
-- Copyright(c)
--          2015-2016 George Washington University
--          2015-2016 University of California Riverside
-- All rights reserved.

-- Redistribution and use in source and binary forms, with or without
-- modification, are permitted provided that the following conditions
-- are met:

-- Redistributions of source code must retain the above copyright
-- notice, this list of conditions and the following disclaimer.
-- Redistributions in binary form must reproduce the above copyright
-- notice, this list of conditions and the following disclaimer in
-- the documentation and/or other materials provided with the
-- distribution.
-- The name of the author may not be used to endorse or promote
-- products derived from this software without specific prior
-- written permission.

-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
-- "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
-- LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
-- A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
-- OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
-- SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
-- LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
-- DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
-- THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
-- (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
-- OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

-- Change any of the settings below to configure Pktgen-DPDK

-- A list of the test script for Pktgen and Lua.
-- Each command somewhat mirrors the pktgen command line versions.
-- A couple of the arguments have be changed to be more like the others.

package.path = package.path ..";?.lua;test/?.lua;app/?.lua;"

require "Pktgen"

printf("Lua Version      : %s\n", pktgen.info.Lua_Version);
printf("Pktgen Version   : %s\n", pktgen.info.Pktgen_Version);
printf("Pktgen Copyright : %s\n", pktgen.info.Pktgen_Copyright);

prints("pktgen.info", pktgen.info);

printf("Port Count %d\n", pktgen.portCount());
printf("Total port Count %d\n", pktgen.totalPorts());


-- set up a mac address to set flow to 
--  
-- TO DO LIST:
--
-- Please update this part with the destination mac address, source and destination ip address you would like to sent packets to 

pktgen.set_mac("0", "90:e2:ba:5e:73:6c"); 
pktgen.set_ipaddr("0", "dst", "10.11.1.17");
pktgen.set_ipaddr("0", "src", "10.11.1.16");

pktgen.set_proto("all", "udp");
pktgen.set_type("all", "ipv4");

pktgen.set("all", "size", 64)
pktgen.set("all", "burst", 32);
pktgen.set("all", "sport", 1234);
pktgen.set("all", "dport", 1234);
pktgen.set("all", "count", 100000000);
pktgen.set("all", "rate",100);

pktgen.vlan_id("all", "start", 1);

