# mesh2x1_rtrportcontrolsn.py
#
# Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
# See LICENSE in the top level directory for licensing details
#
# Exercise RtrPortControlSN + MordredPortLinkSN on the router-to-router
# link of a minimal 2-router (2x1) mesh.
#
# Topology:
#   [testnic_ep_0]              [testnic_ep_1]
#       |                            |
#   (port4)                      (port4)
#   rtr_0_0 ----port1--port3---- rtr_1_0
#   (id=0)    <SN link>          (id=1)
#
# The router-router link uses mordred.rtrPortControlSN (backed by
# mordred.mordredPortLinkSN) on both sides.
# The local endpoint ports fall back to the default rtrPortControl.
#

import sst
from sst import UnitAlgebra

testname = "mesh2x1_rtrportcontrolsn_legacy"

# ---- Simulation parameters ----
clk          = UnitAlgebra("1GHz")
clk_pd       = clk.invert()
link_latency = UnitAlgebra(0.8) * clk_pd
flit_size    = UnitAlgebra("16b")
num_vns      = 1
num_vcs      = 1

PortControlSNParams = {
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

MordredNICParams = {
    "verbose"         : 0,
    "input_buf_size"  : "1kiB",
    "output_buf_size" : "1kiB",
}

# ---- Create routers ----

rtr_0 = sst.Component("rtr_0_0", "mordred.simple_rtr")
rtr_0.addParam("id", 0)
rtr_0.addParams(FixedRtrParams)
rtr_0_topo = rtr_0.setSubComponent("topology", "mordred.MeshTopology")
rtr_0_topo.addParams({"verbose": 0, "xDim": 2, "yDim": 1})

rtr_1 = sst.Component("rtr_1_0", "mordred.simple_rtr")
rtr_1.addParam("id", 1)
rtr_1.addParams(FixedRtrParams)
rtr_1_topo = rtr_1.setSubComponent("topology", "mordred.MeshTopology")
rtr_1_topo.addParams({"verbose": 0, "xDim": 2, "yDim": 1})

# ---- Router-router link via RtrPortControlSN + MordredPortLinkSN ----
#
# rtr_0_0 uses port1 (EAST); rtr_1_0 uses port3 (WEST).
# Slot index must match the port number so SimpleRtr's SubComponentSlotInfo
# lookup by index finds the right control subcomponent.

pc_0 = rtr_0.setSubComponent("portcontrol", "mordred.rtrPortControlSN", 1)
pc_0.addParams(PortControlSNParams)
pif_0 = pc_0.setSubComponent("port_iface", "mordred.mordredPortLinkSN", 0)
pif_0.addParams({"port_name": "port1", "verbose": 0})

pc_1 = rtr_1.setSubComponent("portcontrol", "mordred.rtrPortControlSN", 3)
pc_1.addParams(PortControlSNParams)
pif_1 = pc_1.setSubComponent("port_iface", "mordred.mordredPortLinkSN", 0)
pif_1.addParams({"port_name": "port3", "verbose": 0})

# Physical router-router link
rtr_link = sst.Link("link_rtr0_rtr1")
rtr_0.addLink(rtr_link, "port1", link_latency)
rtr_1.addLink(rtr_link, "port3", link_latency)

# ---- Endpoint 0 on rtr_0_0 (port4 — direct link, default rtrPortControl) ----

ep0 = sst.Component("testnic_ep_0", "merlin.test_nic")
ep0.addParams(FixedTestNicParams)
ep0.addParams({"id": 0, "num_peers": 2})
ep0_iface = ep0.setSubComponent("networkIF", "mordred.mordredNIC")
ep0_iface.addParams(MordredNICParams)

ep0_link = sst.Link("link_ep0_rtr0")
rtr_0.addLink(ep0_link, "port4", link_latency)
ep0_iface.addLink(ep0_link, "port", link_latency)

# ---- Endpoint 1 on rtr_1_0 (port4 — direct link, default rtrPortControl) ----

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
stat_params = {"rate": "0ns"}
sst.setStatisticOutput("sst.statOutputCSV",
    {"filepath": "./stats.%s.csv" % testname, "separator": ", "})
sst.enableAllStatisticsForAllComponents(stat_params)

# EOF
