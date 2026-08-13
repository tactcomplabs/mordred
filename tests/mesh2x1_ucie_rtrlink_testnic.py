# mesh2x1_ucie_rtrlink_testnic.py
#
# Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
# See LICENSE in the top level directory for licensing details
#
# Minimal 2-router (2x1) mesh where the router-router link is a UCIe
# link (RtrPortControlPC + prydwen.uciePhysChannel) but each router's
# endpoint is a plain merlin.test_nic attached via mordred.mordredNIC —
# i.e. no UCIe/PhysChannelAPI anywhere on the test_nic-to-router hop.
# This isolates the UCIe link to the router-router interconnect only.
#
# Every component is run at max mordred verbosity (MORDRED_VERBOSE_ALL,
# see MordredEvents.h) / max prydwen.uciePhysChannel verbosity so the
# full lifecycle of both the UCIe handshake and the plain NoC hops is
# visible in the log. Each test_nic only sends a couple of messages,
# and untimed broadcasts are disabled.
#
# Topology:
#   [testnic_ep_0]                                [testnic_ep_1]
#   (mordredNIC, no UCIe)                          (mordredNIC, no UCIe)
#        |                                                |
#    (port4)                                           (port4)
#    rtr_0_0 ------ port1 -- UCIePhysChannel -- port3 ------ rtr_1_0
#    (id=0)                                                 (id=1)
#

import sst
from sst import UnitAlgebra

testname = "mesh2x1_ucie_rtrlink_testnic"

# ---- Simulation parameters ----
MAXV         = 10  # MORDRED_VERBOSE_ALL (see MordredEvents.h)
clk          = UnitAlgebra("1GHz")
clk_pd       = clk.invert()
link_latency = UnitAlgebra(0.8) * clk_pd
flit_size    = UnitAlgebra("16b")
num_vns      = 1
num_vcs      = 1

UCIeParams = {
    "link_latency"      : "2ns",
    "num_stacks"        : 1,
    "num_vns_per_stack" : "1",
    "credits_per_vn"    : "32",
    "flit_format"       : 5,
    "num_modules"       : 1,
    "num_lanes"         : 16,
    "lane_speed_gts"    : 32,
    "verbose"           : MAXV,
}

PortControlPCParams = {
    "flit_size"       : flit_size,
    "input_buf_size"  : UnitAlgebra(16) * flit_size,
    "output_buf_size" : UnitAlgebra(1)  * flit_size,
    "verbose"         : MAXV,
}

FixedRtrParams = {
    "verbose"         : MAXV,
    "clock"           : clk,
    "num_vcs"         : num_vcs,
    "num_vns"         : num_vns,
    "flit_size"       : flit_size,
    "input_buf_size"  : UnitAlgebra(16) * flit_size,
    "output_buf_size" : UnitAlgebra(1)  * flit_size,
    "num_ports"       : 5,   # N,E,S,W + 1 local
    "num_local_ports" : 1,
}

FixedTestNicParams = {
    "num_messages"           : 2,
    "message_size"           : UnitAlgebra(4) * flit_size,
    "send_untimed_broadcast" : "false",
}

MordredNICParams = {
    "verbose"         : MAXV,
    "clock"           : clk,
    "input_buf_size"  : "1kiB",
    "output_buf_size" : "1kiB",
}

# ---- Create routers ----

rtr_0 = sst.Component("rtr_0_0", "mordred.mordred_router")
rtr_0.addParam("id", 0)
rtr_0.addParams(FixedRtrParams)
rtr_0_topo = rtr_0.setSubComponent("topology", "mordred.MeshTopology")
rtr_0_topo.addParams({"verbose": MAXV, "xDim": 2, "yDim": 1})

rtr_1 = sst.Component("rtr_1_0", "mordred.mordred_router")
rtr_1.addParam("id", 1)
rtr_1.addParams(FixedRtrParams)
rtr_1_topo = rtr_1.setSubComponent("topology", "mordred.MeshTopology")
rtr_1_topo.addParams({"verbose": MAXV, "xDim": 2, "yDim": 1})

# ---- Router-router link via RtrPortControlPC + UCIePhysChannel ----

pc_0 = rtr_0.setSubComponent("portcontrol", "mordred.rtrPortControlPC", 1)
pc_0.addParams(PortControlPCParams)
pif_0 = pc_0.setSubComponent("port_iface", "prydwen.uciePhysChannel", 0)
pif_0.addParams(UCIeParams)
pif_0.addParams({"port_name": "port1", "endpoint_id": 10})

pc_1 = rtr_1.setSubComponent("portcontrol", "mordred.rtrPortControlPC", 3)
pc_1.addParams(PortControlPCParams)
pif_1 = pc_1.setSubComponent("port_iface", "prydwen.uciePhysChannel", 0)
pif_1.addParams(UCIeParams)
pif_1.addParams({"port_name": "port3", "endpoint_id": 11})

rtr_link = sst.Link("link_rtr0_rtr1")
rtr_0.addLink(rtr_link, "port1", link_latency)
rtr_1.addLink(rtr_link, "port3", link_latency)

# ---- Endpoint 0 on rtr_0_0 : plain mordredNIC, no portcontrol subcomponent ----
# (router falls back to an anonymous mordred.rtrPortControl on port4 since no
#  user-configured portcontrol subcomponent is populated at that slot index)

ep0 = sst.Component("testnic_ep_0", "merlin.test_nic")
ep0.addParams(FixedTestNicParams)
ep0.addParams({"id": 0, "num_peers": 2})
ep0_iface = ep0.setSubComponent("networkIF", "mordred.mordredNIC")
ep0_iface.addParams(MordredNICParams)

ep0_link = sst.Link("link_ep0_rtr0")
rtr_0.addLink(ep0_link, "port4", link_latency)
ep0_iface.addLink(ep0_link, "port", link_latency)

# ---- Endpoint 1 on rtr_1_0 : plain mordredNIC, no portcontrol subcomponent ----

ep1 = sst.Component("testnic_ep_1", "merlin.test_nic")
ep1.addParams(FixedTestNicParams)
ep1.addParams({"id": 1, "num_peers": 2})
ep1_iface = ep1.setSubComponent("networkIF", "mordred.mordredNIC")
ep1_iface.addParams(MordredNICParams)

ep1_link = sst.Link("link_ep1_rtr1")
rtr_1.addLink(ep1_link, "port4", link_latency)
ep1_iface.addLink(ep1_link, "port", link_latency)

# ---- Statistics ----

sst.setStatisticLoadLevel(7)
sst.setStatisticOutput("sst.statOutputCSV",
    {"filepath": "./stats.%s.csv" % testname, "separator": ", "})
sst.enableAllStatisticsForAllComponents({"rate": "0ns"})

# EOF
