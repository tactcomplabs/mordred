# mesh3x3_untimed_broadcast.py
#
# Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
# See LICENSE in the top level directory for licensing details
#
# Variant of mesh3x3_testnic.py with send_untimed_broadcast=true.
#
# This is the only test that exercises the routeUntimedBroadcastPacket()
# path in MeshTopology (and the untimedInitEventsQ in MordredRouter).
# merlin.test_nic sends broadcast packets during SST init/complete phases
# when send_untimed_broadcast=true; all routers must flood them correctly
# before timed simulation begins.
#

import sst
from sst import UnitAlgebra

testname = "mesh3x3_untimed_broadcast"

clk          = UnitAlgebra("1GHz")
clk_pd       = clk.invert()
link_latency = UnitAlgebra(0.8) * clk_pd
flit_size    = UnitAlgebra("16b")

FixedRtrParams = {
    "verbose"         : 0,
    "clock"           : clk,
    "num_vcs"         : "1",
    "num_vns"         : "1",
    "flit_size"       : flit_size,
    "input_buf_size"  : UnitAlgebra(16) * flit_size,
    "output_buf_size" : UnitAlgebra(1)  * flit_size,
}

FixedTestNicParams = {
    "num_messages"           : 10,
    "message_size"           : UnitAlgebra(4) * flit_size,
    "send_untimed_broadcast" : "true",
}

MordredNICParams = {
    "verbose"         : 0,
    "input_buf_size"  : "1kiB",
    "output_buf_size" : "1kiB",
}

links = dict()
def getLink(name1, name2):
    name = "link.%s_%s" % (name1, name2)
    if name not in links:
        links[name] = sst.Link(name)
    return links[name]

def createMesh(x_size, y_size, local_ports):
    rtr_id = 0
    nports = 4 + local_ports
    rtr_params = {
        "num_ports"       : nports,
        "num_local_ports" : local_ports,
    }
    for y in range(y_size):
        for x in range(x_size):
            rtr = sst.Component("rtr_%d_%d" % (x, y), "mordred.mordred_router")
            rtr.addParam("id", rtr_id)
            rtr.addParams(FixedRtrParams)
            rtr.addParams(rtr_params)
            rtr_id += 1
            rtr_topo = rtr.setSubComponent("topology", "mordred.MeshTopology")
            rtr_topo.addParams({"verbose": 0, "xDim": x_size, "yDim": y_size})

            if y != y_size - 1:
                rtr.addLink(getLink("rtr_%d_%d" % (x, y), "rtr_%d_%d" % (x, y + 1)), "port0", link_latency)
            if x != x_size - 1:
                rtr.addLink(getLink("rtr_%d_%d" % (x, y), "rtr_%d_%d" % (x + 1, y)), "port1", link_latency)
            if y != 0:
                rtr.addLink(getLink("rtr_%d_%d" % (x, y - 1), "rtr_%d_%d" % (x, y)), "port2", link_latency)
            if x != 0:
                rtr.addLink(getLink("rtr_%d_%d" % (x - 1, y), "rtr_%d_%d" % (x, y)), "port3", link_latency)

            for k in range(local_ports):
                lcl_portname = "port%d" % (k + 4)
                ep_name = "testnic_ep_%d_%d_%d" % (x, y, k)
                ep_num  = (x * y_size * local_ports) + (y * local_ports) + k
                num_eps = x_size * y_size * local_ports

                ep = sst.Component(ep_name, "merlin.test_nic")
                ep.addParams(FixedTestNicParams)
                ep.addParams({"id": ep_num, "num_peers": num_eps})

                ep_iface = ep.setSubComponent("networkIF", "mordred.mordredNIC")
                ep_iface.addParams(MordredNICParams)

                rtr.addLink(getLink("rtr_%d_%d" % (x, y), ep_name), lcl_portname, link_latency)
                ep_iface.addLink(getLink("rtr_%d_%d" % (x, y), ep_name), "port", link_latency)

local_ports = 1
x_size      = 3
y_size      = 3

createMesh(x_size, y_size, local_ports)

sst.setStatisticLoadLevel(7)
stat_params = {"rate": "0ns"}
sst.setStatisticOutput("sst.statOutputCSV",
    {"filepath": "./stats.%s.csv" % testname, "separator": ", "})
sst.enableAllStatisticsForAllComponents(stat_params)

# EOF
