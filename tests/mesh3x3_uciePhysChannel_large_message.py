# mesh3x3_uciePhysChannel_large_message.py
#
# Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
# See LICENSE in the top level directory for licensing details
#
# Large-message variant of mesh3x3_uciePhysChannel.py — the first test in
# either repo to actually exercise UCIePhysChannel's multi-fragment path.
#
# Every other *_uciePhysChannel*.py test uses an 8-byte message (4 flits at
# flit_size=16b), which always fits in a single physical UCIe flit regardless
# of format — so p_count is always 1 and the FlitFactory's m_count>1
# reassembly branch never runs. This test uses flit_size=32b (Mordred's
# documented router default) and a 1024-byte message — exactly the scenario
# from docs/1024b-packet-walkthrough.html:
#
#   calcNumFlits(1024B*8 / 32b)        = 256 Mordred flits (1 HEAD, 254 BODY, 1 TAIL)
#   ceil(1024B / 240B flit_format-5 payload) = 5 physical UCIe FLITs
#
# i.e. coalescing should turn 256 naive per-flit sends into 5 physical wire
# transfers. Check stats.mesh3x3_uciePhysChannel_large_message.csv afterward:
# prydwen's `flits_sent` should be on the order of 5 per message (not 256),
# and mordred's `average_noc_latency` should be a small, sane number (not
# UINT64_MAX-scale garbage — this also validates the head_inject_cycle fix).
#
# input_buf_size/output_buf_size are sized well above the 1x-flit_size
# minimum used elsewhere: coalescing adds real per-hop credit-turnaround
# latency (see RtrPortControlPC.h's pendingFlits_ doc), so a message this
# large needs headroom to avoid stalling — same lesson as the torus tests.

import sst
from sst import UnitAlgebra

testname = "mesh3x3_uciePhysChannel_large_message"

clk          = UnitAlgebra("1GHz")
clk_pd       = clk.invert()
link_latency = UnitAlgebra(0.8) * clk_pd
flit_size    = UnitAlgebra("32b")

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
    "input_buf_size"  : UnitAlgebra(64)  * flit_size,
    "output_buf_size" : UnitAlgebra(32)  * flit_size,
    "verbose"         : 0,
}

FixedRtrParams = {
    "verbose"         : 0,
    "clock"           : clk,
    "num_vcs"         : "1",
    "num_vns"         : "1",
    "flit_size"       : flit_size,
    "input_buf_size"  : UnitAlgebra(64)  * flit_size,
    "output_buf_size" : UnitAlgebra(32)  * flit_size,
}

FixedTestNicParams = {
    "num_messages"           : 4,
    "message_size"           : "1024B",
    "send_untimed_broadcast" : "false",
}

MordredNicPCParams = {
    "verbose"         : 0,
    "input_buf_size"  : "4kiB",
    "output_buf_size" : "4kiB",
}

links = dict()
def getLink(name1, name2):
    name = "link.%s_%s" % (name1, name2)
    if name not in links:
        links[name] = sst.Link(name)
    return links[name]

def wire_rtr_port(rtr, port_idx, port_name, link, ucie_ep_id):
    """Attach rtrPortControlPC + UCIePhysChannel to one router port and connect the link."""
    pc = rtr.setSubComponent("portcontrol", "mordred.rtrPortControlPC", port_idx)
    pc.addParams(PortControlPCParams)
    ucie = pc.setSubComponent("port_iface", "prydwen.uciePhysChannel", 0)
    ucie.addParams(UCIeParams)
    ucie.addParams({"port_name": port_name, "endpoint_id": ucie_ep_id})
    rtr.addLink(link, port_name, link_latency)

def createMesh(x_size, y_size, local_ports):
    nports = 4 + local_ports
    rtr_params = {
        "num_ports"       : nports,
        "num_local_ports" : local_ports,
    }
    num_eps = x_size * y_size * local_ports

    for y in range(y_size):
        for x in range(x_size):
            rtr_id = y * x_size + x
            rtr = sst.Component("rtr_%d_%d" % (x, y), "mordred.mordred_router")
            rtr.addParam("id", rtr_id)
            rtr.addParams(FixedRtrParams)
            rtr.addParams(rtr_params)
            rtr_topo = rtr.setSubComponent("topology", "mordred.MeshTopology")
            rtr_topo.addParams({"verbose": 0, "xDim": x_size, "yDim": y_size})

            # North (port0): (x,y) → (x,y+1)
            if y != y_size - 1:
                lnk = getLink("rtr_%d_%d" % (x, y), "rtr_%d_%d" % (x, y + 1))
                wire_rtr_port(rtr, 0, "port0", lnk, 2000 + rtr_id * 10 + 0)

            # East (port1): (x,y) → (x+1,y)
            if x != x_size - 1:
                lnk = getLink("rtr_%d_%d" % (x, y), "rtr_%d_%d" % (x + 1, y))
                wire_rtr_port(rtr, 1, "port1", lnk, 2000 + rtr_id * 10 + 1)

            # South (port2): reverse of north — connect to the link created by (x,y-1)
            if y != 0:
                lnk = getLink("rtr_%d_%d" % (x, y - 1), "rtr_%d_%d" % (x, y))
                wire_rtr_port(rtr, 2, "port2", lnk, 2000 + rtr_id * 10 + 2)

            # West (port3): reverse of east — connect to the link created by (x-1,y)
            if x != 0:
                lnk = getLink("rtr_%d_%d" % (x - 1, y), "rtr_%d_%d" % (x, y))
                wire_rtr_port(rtr, 3, "port3", lnk, 2000 + rtr_id * 10 + 3)

            # Local ports (port4+)
            for k in range(local_ports):
                port_idx  = 4 + k
                port_name = "port%d" % port_idx
                ep_name   = "testnic_ep_%d_%d_%d" % (x, y, k)
                ep_num    = (x * y_size * local_ports) + (y * local_ports) + k

                ep = sst.Component(ep_name, "merlin.test_nic")
                ep.addParams(FixedTestNicParams)
                ep.addParams({"id": ep_num, "num_peers": num_eps})

                lnk = getLink("rtr_%d_%d" % (x, y), ep_name)

                # Router-side port
                wire_rtr_port(rtr, port_idx, port_name, lnk, 1000 + rtr_id * 10 + port_idx)

                # NIC-side
                ep_iface = ep.setSubComponent("networkIF", "mordred.mordredNicPC")
                ep_iface.addParams(MordredNicPCParams)
                ucie_ep = ep_iface.setSubComponent("port_iface", "prydwen.uciePhysChannel", 0)
                ucie_ep.addParams(UCIeParams)
                ucie_ep.addParams({"port_name": "port", "endpoint_id": ep_num})
                ep_iface.addLink(lnk, "port", link_latency)

local_ports = 1
x_size      = 3
y_size      = 3

createMesh(x_size, y_size, local_ports)

sst.setStatisticLoadLevel(7)
sst.setStatisticOutput("sst.statOutputCSV",
    {"filepath": "./stats.%s.csv" % testname, "separator": ", "})
sst.enableAllStatisticsForComponentType("prydwen.uciePhysChannel")
sst.enableAllStatisticsForComponentType("mordred.mordredNicPC")

# EOF
