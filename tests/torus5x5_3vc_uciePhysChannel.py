# torus5x5_3vc_uciePhysChannel.py
#
# Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
# See LICENSE in the top level directory for licensing details
#
# 3-VC uciePhysChannel variant of torus5x5_2vc_uciePhysChannel.py.
#
# Every link in the 5x5 torus uses prydwen.uciePhysChannel as the physical
# transport.  VCs (num_vcs=3) are managed above the port-control layer, so
# the UCIe channel uses a single VN (num_vns_per_stack="1").  Topology and
# traffic parameters are identical to the 2vc original.
#
# NOTE: num_vcs >= 2 is required on a 5x5 (or larger) torus to avoid deadlock.
# This test exercises VcAllocRR with 3 VCs: VC0 for non-wrap links,
# VC1/VC2 for wrap-around links (only VC1 is used by wrap logic; VC2 is
# available for future use and exercises the VC count boundary in VcAllocRR).
#

import sst
from sst import UnitAlgebra

testname = "torus5x5_3vc_uciePhysChannel"

clk          = UnitAlgebra("1GHz")
clk_pd       = clk.invert()
link_latency = UnitAlgebra(0.8) * clk_pd
flit_size    = UnitAlgebra("16b")

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
    "num_vcs"         : "3",
    "num_vns"         : "1",
    "flit_size"       : flit_size,
    "input_buf_size"  : UnitAlgebra(16) * flit_size,
    "output_buf_size" : UnitAlgebra(1)  * flit_size,
}

FixedTestNicParams = {
    "num_messages"           : 10,
    "message_size"           : UnitAlgebra(4) * flit_size,
    "send_untimed_broadcast" : "false",
}

MordredNicPCParams = {
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

def wire_rtr_port(rtr, port_idx, port_name, link, ucie_ep_id):
    """Attach rtrPortControlPC + UCIePhysChannel to one router port and connect the link."""
    pc = rtr.setSubComponent("portcontrol", "mordred.rtrPortControlPC", port_idx)
    pc.addParams(PortControlPCParams)
    ucie = pc.setSubComponent("port_iface", "prydwen.uciePhysChannel", 0)
    ucie.addParams(UCIeParams)
    ucie.addParams({"port_name": port_name, "endpoint_id": ucie_ep_id})
    rtr.addLink(link, port_name, link_latency)

def createTorus(x_size, y_size, local_ports):
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
            rtr_topo = rtr.setSubComponent("topology", "mordred.torusTopo")
            rtr_topo.addParams({"verbose": 0, "xDim": x_size, "yDim": y_size})

            # North (port0)
            if y != y_size - 1:
                lnk = getLink("rtr_%d_%d" % (x, y), "rtr_%d_%d" % (x, y + 1))
            else:
                lnk = getLink("rtr_%d_%d" % (x, y), "rtr_%d_%d" % (x, 0))
            wire_rtr_port(rtr, 0, "port0", lnk, 2000 + rtr_id * 10 + 0)

            # East (port1)
            if x != x_size - 1:
                lnk = getLink("rtr_%d_%d" % (x, y), "rtr_%d_%d" % (x + 1, y))
            else:
                lnk = getLink("rtr_%d_%d" % (x, y), "rtr_%d_%d" % (0, y))
            wire_rtr_port(rtr, 1, "port1", lnk, 2000 + rtr_id * 10 + 1)

            # South (port2) — reverse of north
            if y != 0:
                lnk = getLink("rtr_%d_%d" % (x, y - 1), "rtr_%d_%d" % (x, y))
            else:
                lnk = getLink("rtr_%d_%d" % (x, y_size - 1), "rtr_%d_%d" % (x, 0))
            wire_rtr_port(rtr, 2, "port2", lnk, 2000 + rtr_id * 10 + 2)

            # West (port3) — reverse of east
            if x != 0:
                lnk = getLink("rtr_%d_%d" % (x - 1, y), "rtr_%d_%d" % (x, y))
            else:
                lnk = getLink("rtr_%d_%d" % (x_size - 1, y), "rtr_%d_%d" % (0, y))
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
x_size      = 5
y_size      = 5

createTorus(x_size, y_size, local_ports)

sst.setStatisticLoadLevel(7)
sst.setStatisticOutput("sst.statOutputCSV",
    {"filepath": "./stats.%s.csv" % testname, "separator": ", "})
sst.enableAllStatisticsForComponentType("prydwen.uciePhysChannel")

# EOF
