# mesh2x1_rtrportcontrolpc.py
#
# Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
# See LICENSE in the top level directory for licensing details
#
# Exercise RtrPortControlPC + GenericPhysChannel (router side) and
# MordredNicPC + GenericPhysChannel (endpoint side) on a minimal 2-router
# (2x1) mesh.
#
# Both ends of every link use the PhysChannelAPI-backed path.  The physical
# link carries PhysChannelLinkEvent objects (the GenericPhysChannel wire
# format) rather than raw baseMordredEvent objects.
#
# Topology:
#   [testnic_ep_0]              [testnic_ep_1]
#       |                            |
#   (port4)                      (port4)
#   rtr_0_0 ----port1--port3---- rtr_1_0
#   (id=0)   <passthrough pc>    (id=1)
#
#

import sst
from sst import UnitAlgebra

testname = "mesh2x1_rtrportcontrolpc"

# ---- Simulation parameters ----
clk          = UnitAlgebra("1GHz")
clk_pd       = clk.invert()
link_latency = UnitAlgebra(0.8) * clk_pd
flit_size    = UnitAlgebra("16b")
num_vns      = 1
num_vcs      = 1

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

# ---- Router-router link via RtrPortControlPC + GenericPhysChannel ----
#
# rtr_0_0 uses port1 (EAST); rtr_1_0 uses port3 (WEST).
# Slot index must match the port number.

pc_0 = rtr_0.setSubComponent("portcontrol", "mordred.rtrPortControlPC", 1)
pc_0.addParams(PortControlPCParams)
pif_0 = pc_0.setSubComponent("port_iface", "prydwen.genericPhysChannel", 0)
pif_0.addParams({"port_name": "port1", "verbose": 0})

pc_1 = rtr_1.setSubComponent("portcontrol", "mordred.rtrPortControlPC", 3)
pc_1.addParams(PortControlPCParams)
pif_1 = pc_1.setSubComponent("port_iface", "prydwen.genericPhysChannel", 0)
pif_1.addParams({"port_name": "port3", "verbose": 0})

# Physical router-router link
rtr_link = sst.Link("link_rtr0_rtr1")
rtr_0.addLink(rtr_link, "port1", link_latency)
rtr_1.addLink(rtr_link, "port3", link_latency)

# ---- Endpoint 0 on rtr_0_0 via RtrPortControlPC + GenericPhysChannel (router side)
#      and MordredNicPC + GenericPhysChannel (endpoint side) ----
#
# IMPORTANT: both ends of every link must use the same wire format.
# GenericPhysChannel puts PhysChannelLinkEvent objects on the link.
# Therefore the router's local port (port4) must also use RtrPortControlPC +
# GenericPhysChannel — the legacy RtrPortControl fallback is NOT used here.
#
# The physical link name on MordredNicPC is "port" (from SST_ELI_DOCUMENT_PORTS);
# the inner PassthroughPC accesses it via SHARE_PORTS.

pc_0_ep = rtr_0.setSubComponent("portcontrol", "mordred.rtrPortControlPC", 4)
pc_0_ep.addParams(PortControlPCParams)
pif_0_ep = pc_0_ep.setSubComponent("port_iface", "prydwen.genericPhysChannel", 0)
pif_0_ep.addParams({"port_name": "port4", "verbose": 0})

ep0 = sst.Component("testnic_ep_0", "merlin.test_nic")
ep0.addParams(FixedTestNicParams)
ep0.addParams({"id": 0, "num_peers": 2})
ep0_iface = ep0.setSubComponent("networkIF", "mordred.mordredNicPC")
ep0_iface.addParams(MordredNicPCParams)
ep0_pc = ep0_iface.setSubComponent("port_iface", "prydwen.genericPhysChannel", 0)
ep0_pc.addParams({"port_name": "port", "verbose": 0})

ep0_link = sst.Link("link_ep0_rtr0")
rtr_0.addLink(ep0_link, "port4", link_latency)
ep0_iface.addLink(ep0_link, "port", link_latency)

# ---- Endpoint 1 on rtr_1_0 (same pattern) ----

pc_1_ep = rtr_1.setSubComponent("portcontrol", "mordred.rtrPortControlPC", 4)
pc_1_ep.addParams(PortControlPCParams)
pif_1_ep = pc_1_ep.setSubComponent("port_iface", "prydwen.genericPhysChannel", 0)
pif_1_ep.addParams({"port_name": "port4", "verbose": 0})

ep1 = sst.Component("testnic_ep_1", "merlin.test_nic")
ep1.addParams(FixedTestNicParams)
ep1.addParams({"id": 1, "num_peers": 2})
ep1_iface = ep1.setSubComponent("networkIF", "mordred.mordredNicPC")
ep1_iface.addParams(MordredNicPCParams)
ep1_pc = ep1_iface.setSubComponent("port_iface", "prydwen.genericPhysChannel", 0)
ep1_pc.addParams({"port_name": "port", "verbose": 0})

ep1_link = sst.Link("link_ep1_rtr1")
rtr_1.addLink(ep1_link, "port4", link_latency)
ep1_iface.addLink(ep1_link, "port", link_latency)

# ---- Statistics ----

sst.setStatisticLoadLevel(7)
stat_params = {"rate": "0ns"}
sst.setStatisticOutput("sst.statOutputCSV",
    {"filepath": "./stats.%s.csv" % testname, "separator": ", "})
sst.enableAllStatisticsForAllComponents(stat_params)

# EOF
