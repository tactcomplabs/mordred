# mesh2x1_uciePhysChannel.py
#
# Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
# See LICENSE in the top level directory for licensing details
#
# Exercise RtrPortControlPC + UCIePhysChannel (router side) and
# MordredNicPC + UCIePhysChannel (endpoint side) on a minimal 2-router
# (2x1) mesh.
#
# This is a uciePhysChannel variant of mesh2x1_rtrportcontrolpc.py.
# The physical link now carries UCIePhysDataFlit objects and each link
# performs a UCIe Init/Agree handshake during SST's untimed init phases.
#
# Topology:
#   [testnic_ep_0]              [testnic_ep_1]
#       |                            |
#   (port4)                      (port4)
#   rtr_0_0 ----port1--port3---- rtr_1_0
#   (id=0)   <UCIePhysChannel>   (id=1)
#

import sst
from sst import UnitAlgebra

testname = "mesh2x1_uciePhysChannel"

# ---- Simulation parameters ----
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
    "verbose"           : 1,
}

PortControlPCParams = {
    "flit_size"       : flit_size,
    "input_buf_size"  : UnitAlgebra(16) * flit_size,
    "output_buf_size" : UnitAlgebra(1)  * flit_size,
    "verbose"         : 0,
}

FixedRtrParams = {
    "verbose"         : 0,
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
    "num_messages" : 10,
    "message_size" : UnitAlgebra(4) * flit_size,
    "send_untimed_broadcast" : "false",
}

MordredNicPCParams = {
    "verbose"         : 0,
    "input_buf_size"  : "1kiB",
    "output_buf_size" : "1kiB",
}

# ---- Create routers ----

rtr_0 = sst.Component("rtr_0_0", "mordred.mordred_router")
rtr_0.addParam("id", 0)
rtr_0.addParams(FixedRtrParams)
rtr_0_topo = rtr_0.setSubComponent("topology", "mordred.MeshTopology")
rtr_0_topo.addParams({"verbose": 0, "xDim": 2, "yDim": 1})

rtr_1 = sst.Component("rtr_1_0", "mordred.mordred_router")
rtr_1.addParam("id", 1)
rtr_1.addParams(FixedRtrParams)
rtr_1_topo = rtr_1.setSubComponent("topology", "mordred.MeshTopology")
rtr_1_topo.addParams({"verbose": 0, "xDim": 2, "yDim": 1})

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

# ---- Endpoint 0 on rtr_0_0 ----

pc_0_ep = rtr_0.setSubComponent("portcontrol", "mordred.rtrPortControlPC", 4)
pc_0_ep.addParams(PortControlPCParams)
pif_0_ep = pc_0_ep.setSubComponent("port_iface", "prydwen.uciePhysChannel", 0)
pif_0_ep.addParams(UCIeParams)
pif_0_ep.addParams({"port_name": "port4", "endpoint_id": 20})

ep0 = sst.Component("testnic_ep_0", "merlin.test_nic")
ep0.addParams(FixedTestNicParams)
ep0.addParams({"id": 0, "num_peers": 2})
ep0_iface = ep0.setSubComponent("networkIF", "mordred.mordredNicPC")
ep0_iface.addParams(MordredNicPCParams)
ep0_pc = ep0_iface.setSubComponent("port_iface", "prydwen.uciePhysChannel", 0)
ep0_pc.addParams(UCIeParams)
ep0_pc.addParams({"port_name": "port", "endpoint_id": 0})

ep0_link = sst.Link("link_ep0_rtr0")
rtr_0.addLink(ep0_link, "port4", link_latency)
ep0_iface.addLink(ep0_link, "port", link_latency)

# ---- Endpoint 1 on rtr_1_0 ----

pc_1_ep = rtr_1.setSubComponent("portcontrol", "mordred.rtrPortControlPC", 4)
pc_1_ep.addParams(PortControlPCParams)
pif_1_ep = pc_1_ep.setSubComponent("port_iface", "prydwen.uciePhysChannel", 0)
pif_1_ep.addParams(UCIeParams)
pif_1_ep.addParams({"port_name": "port4", "endpoint_id": 21})

ep1 = sst.Component("testnic_ep_1", "merlin.test_nic")
ep1.addParams(FixedTestNicParams)
ep1.addParams({"id": 1, "num_peers": 2})
ep1_iface = ep1.setSubComponent("networkIF", "mordred.mordredNicPC")
ep1_iface.addParams(MordredNicPCParams)
ep1_pc = ep1_iface.setSubComponent("port_iface", "prydwen.uciePhysChannel", 0)
ep1_pc.addParams(UCIeParams)
ep1_pc.addParams({"port_name": "port", "endpoint_id": 1})

ep1_link = sst.Link("link_ep1_rtr1")
rtr_1.addLink(ep1_link, "port4", link_latency)
ep1_iface.addLink(ep1_link, "port", link_latency)

# ---- Statistics ----

sst.setStatisticLoadLevel(7)
sst.setStatisticOutput("sst.statOutputCSV",
    {"filepath": "./stats.%s.csv" % testname, "separator": ", "})
sst.enableAllStatisticsForComponentType("prydwen.uciePhysChannel")

# EOF
