# torus3D_3x3x3_2vc_uciePhysChannel.py
#
# Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
# See LICENSE in the top level directory for licensing details
#
# uciePhysChannel variant of torus3D_3x3x3_2vc_testnic.py.
#
# Every link in the 3x3x3 3-D torus uses prydwen.uciePhysChannel as the
# physical transport.  VCs (num_vcs=2) are managed above the port-control
# layer, so the UCIe channel uses a single VN (num_vns_per_stack="1").
# Topology and traffic parameters are identical to the original.
#
# Port assignment per router (mordred.torus3DTopo convention):
#   port0 = North (+Y), port1 = East (+X)
#   port2 = South (-Y), port3 = West (-X)
#   port4 = +Z,         port5 = -Z
#   port6+ = local endpoints
#

import sst
from sst import UnitAlgebra

testname = "torus3D_3x3x3_2vc_uciePhysChannel"

clk          = UnitAlgebra("1GHz")
clk_pd       = clk.invert()
link_latency = UnitAlgebra(0.8) * clk_pd
flit_size    = UnitAlgebra("16b")
num_vns      = "1"

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
    "verbose"         : "0",
    "clock"           : clk,
    "num_vcs"         : "2",
    "num_vns"         : num_vns,
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
    "verbose"         : "0",
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

def createTorus(x_size, y_size, z_size, local_ports):
    nports = 6 + local_ports
    rtr_params = {
        "num_ports"       : nports,
        "num_local_ports" : local_ports,
    }
    num_eps = x_size * y_size * z_size * local_ports
    rtr_id  = 0

    for z in range(z_size):
        for y in range(y_size):
            for x in range(x_size):
                rtr = sst.Component("rtr_%d_%d_%d" % (x, y, z), "mordred.mordred_router")
                rtr.addParam("id", rtr_id)
                rtr.addParams(FixedRtrParams)
                rtr.addParams(rtr_params)
                rtr_topo = rtr.setSubComponent("topology", "mordred.torus3DTopo")
                rtr_topo.addParams({
                    "verbose" : 0,
                    "xDim"   : x_size,
                    "yDim"   : y_size,
                    "zDim"   : z_size,
                })

                # North (port0, +Y)
                if y != y_size - 1:
                    lnk = getLink("rtr_%d_%d_%d" % (x, y, z), "rtr_%d_%d_%d" % (x, y + 1, z))
                else:
                    lnk = getLink("rtr_%d_%d_%d" % (x, y, z), "rtr_%d_%d_%d" % (x, 0, z))
                wire_rtr_port(rtr, 0, "port0", lnk, 2000 + rtr_id * 10 + 0)

                # East (port1, +X)
                if x != x_size - 1:
                    lnk = getLink("rtr_%d_%d_%d" % (x, y, z), "rtr_%d_%d_%d" % (x + 1, y, z))
                else:
                    lnk = getLink("rtr_%d_%d_%d" % (x, y, z), "rtr_%d_%d_%d" % (0, y, z))
                wire_rtr_port(rtr, 1, "port1", lnk, 2000 + rtr_id * 10 + 1)

                # South (port2, -Y) — reverse of north
                if y != 0:
                    lnk = getLink("rtr_%d_%d_%d" % (x, y - 1, z), "rtr_%d_%d_%d" % (x, y, z))
                else:
                    lnk = getLink("rtr_%d_%d_%d" % (x, y_size - 1, z), "rtr_%d_%d_%d" % (x, 0, z))
                wire_rtr_port(rtr, 2, "port2", lnk, 2000 + rtr_id * 10 + 2)

                # West (port3, -X) — reverse of east
                if x != 0:
                    lnk = getLink("rtr_%d_%d_%d" % (x - 1, y, z), "rtr_%d_%d_%d" % (x, y, z))
                else:
                    lnk = getLink("rtr_%d_%d_%d" % (x_size - 1, y, z), "rtr_%d_%d_%d" % (0, y, z))
                wire_rtr_port(rtr, 3, "port3", lnk, 2000 + rtr_id * 10 + 3)

                # +Z (port4)
                if z != z_size - 1:
                    lnk = getLink("rtr_%d_%d_%d" % (x, y, z), "rtr_%d_%d_%d" % (x, y, z + 1))
                else:
                    lnk = getLink("rtr_%d_%d_%d" % (x, y, z), "rtr_%d_%d_%d" % (x, y, 0))
                wire_rtr_port(rtr, 4, "port4", lnk, 2000 + rtr_id * 10 + 4)

                # -Z (port5) — reverse of +Z
                if z != 0:
                    lnk = getLink("rtr_%d_%d_%d" % (x, y, z - 1), "rtr_%d_%d_%d" % (x, y, z))
                else:
                    lnk = getLink("rtr_%d_%d_%d" % (x, y, z_size - 1), "rtr_%d_%d_%d" % (x, y, 0))
                wire_rtr_port(rtr, 5, "port5", lnk, 2000 + rtr_id * 10 + 5)

                # Local ports (port6+)
                for k in range(local_ports):
                    port_idx  = 6 + k
                    port_name = "port%d" % port_idx
                    ep_name   = "testnic_ep_%d_%d_%d_%d" % (x, y, z, k)
                    ep_num    = rtr_id * local_ports + k

                    ep = sst.Component(ep_name, "merlin.test_nic")
                    ep.addParams(FixedTestNicParams)
                    ep.addParams({"id": ep_num, "num_peers": num_eps})

                    lnk = getLink("rtr_%d_%d_%d" % (x, y, z), ep_name)

                    # Router-side port
                    wire_rtr_port(rtr, port_idx, port_name, lnk,
                                  1000 + rtr_id * 10 + port_idx)

                    # NIC-side
                    ep_iface = ep.setSubComponent("networkIF", "mordred.mordredNicPC")
                    ep_iface.addParams(MordredNicPCParams)
                    ucie_ep = ep_iface.setSubComponent("port_iface", "prydwen.uciePhysChannel", 0)
                    ucie_ep.addParams(UCIeParams)
                    ucie_ep.addParams({"port_name": "port", "endpoint_id": ep_num})
                    ep_iface.addLink(lnk, "port", link_latency)

                rtr_id += 1

local_ports = 1
x_size      = 3
y_size      = 3
z_size      = 3

createTorus(x_size, y_size, z_size, local_ports)

sst.setStatisticLoadLevel(7)
sst.setStatisticOutput("sst.statOutputCSV",
    {"filepath": "./stats.%s.csv" % testname, "separator": ", "})
sst.enableAllStatisticsForComponentType("prydwen.uciePhysChannel")

# EOF
