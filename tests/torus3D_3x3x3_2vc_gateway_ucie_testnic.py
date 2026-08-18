# torus3D_3x3x3_2vc_gateway_ucie_testnic.py
#
# Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
# See LICENSE in the top level directory for licensing details
#
# Torus counterpart to mesh2x2_gateway_ucie_testnic.py: two INDEPENDENTLY-
# ADDRESSED 3x3x3 tori ("torus A" and "torus B"), each a self-contained
# mordred.torus3DTopo (local ids 0-26), joined by a single UCIe link that
# carries ALL cross-torus traffic. As with the mesh case, this is possible
# because every router loads mordred.GatewayTopology as its "topology"
# subcomponent, wrapping mordred.torus3DTopo in the "inner_topology" slot;
# each torus owns a contiguous slice of one shared global id space (torus A:
# ids 0-26, torus B: ids 27-53, via id_base), and a destination outside a
# router's own torus is redirected toward that torus's one designated
# gateway router. See GatewayTopology.h for the full design rationale.
#
# This is the topology the "additive port, never a repurposed cardinal/
# wraparound direction" rule in GatewayTopology exists for: Torus3DTopo
# fatals in init() if ANY of its 6 router-router ports is left unconnected
# (see Torus3dTopo.cc's init()), because every one of them is a real
# wraparound link that adaptive shortest-path routing actively uses (see
# Torus3DTopo::routePacket()'s dx_pos/dx_neg comparison) -- unlike a mesh
# boundary, there is no such thing as an idle torus port to steal. So the
# one designated gateway router in each torus (torusA_rtr_0_0_0 and
# torusB_rtr_0_0_0) gets a genuinely extra 8th port (index 7: 6 cardinal +
# 1 local + 1 gateway) for the UCIe link; every other router keeps the
# normal 7 ports (6 cardinal + 1 local), fully wraparound-connected, exactly
# like torus3D_3x3x3_2vc_testnic.py.
#
# Every test_nic-to-router hop is still a plain mordred.mordredNIC link --
# no UCIe/PhysChannelAPI anywhere except the one gateway-to-gateway link.
#
# Every test_nic addresses num_peers=54 (the full global space: 27 nodes/
# torus x 2 tori), so real cross-torus traffic is generated and is
# guaranteed -- by construction, not by observation -- to funnel entirely
# through the single gateway link.
#
# num_vcs=2 (not 1) throughout, matching every other torus3D test in this
# suite: a torus's wraparound links can deadlock dimension-order routing
# with only one VC, so a second (escape) VC is required regardless of the
# gateway -- this isn't gateway-specific, it's baseline torus correctness.
#

import sst
from sst import UnitAlgebra

testname = "torus3D_3x3x3_2vc_gateway_ucie_testnic"

# ---- Simulation parameters ----
# Kept at MORDRED_VERBOSE_MIN, not MORDRED_VERBOSE_ALL like the smaller
# gateway tests: at max verbosity this test's ~1500-packet gateway link and
# 54 routers produce a ~150MB log for a ~5s simulation, which isn't a useful
# trade for a regression test. Bump to MORDRED_VERBOSE_HIGH/ALL locally when
# actually debugging this test.
MINV              = 1  # MORDRED_VERBOSE_MIN (see MordredEvents.h)
clk               = UnitAlgebra("1GHz")
clk_pd            = clk.invert()
noc_link_latency  = UnitAlgebra(0.8) * clk_pd
flit_size         = UnitAlgebra("16b")
num_vns           = 1
num_vcs           = 2   # torus wraparound needs an escape VC -- see module docstring
ucie_link_latency = "2ns"

UCIeParams = {
    "link_latency"      : ucie_link_latency,
    "num_stacks"        : 1,
    "num_vns_per_stack" : "2",  # numVns(1) * numVcs(2) -- see RtrPortControlPC::extVn()
    "credits_per_vn"    : "64",
    "flit_format"       : 5,
    "num_modules"       : 1,
    "num_lanes"         : 16,
    "lane_speed_gts"    : 32,
    "verbose"           : MINV,
}

PortControlPCParams = {
    "flit_size"       : flit_size,
    "input_buf_size"  : UnitAlgebra(32) * flit_size,
    "output_buf_size" : UnitAlgebra(2)  * flit_size,
    "verbose"         : MINV,
}

FixedTestNicParams = {
    "num_messages"           : 2,
    "message_size"           : UnitAlgebra(4) * flit_size,
    "send_untimed_broadcast" : "false",
}

MordredNICParams = {
    "verbose"         : MINV,
    "clock"           : clk,
    "input_buf_size"  : "1kiB",
    "output_buf_size" : "1kiB",
}

# ---- Torus configuration ----
x_size = 3
y_size = 3
z_size = 3
local_range_size = x_size * y_size * z_size  # 1 local port per router -> 27 endpoints/torus

links = dict()
def getLink(name1, name2):
    name = "link.%s_%s" % (name1, name2)
    if name not in links:
        links[name] = sst.Link(name)
    return links[name]

def build_torus(torus_name, id_base, remotes):
    """Build one self-contained 3x3x3 torus; returns its routers keyed by (x,y,z).

    torus_name -- prefix for component names (e.g. "torusA")
    id_base    -- this torus's offset into the shared global id space
    remotes    -- list of dicts, one per directly-linked remote domain:
                  {"id_base":, "range_size":, "gateway_rtr_id":, "gateway_port":}
                  gateway_rtr_id is the LOCAL rtr_id (0..26) of THIS torus's
                  designated gateway router to that remote domain.
    """
    routers = {}
    remote_id_bases        = ",".join(str(r["id_base"])        for r in remotes)
    remote_range_sizes     = ",".join(str(r["range_size"])     for r in remotes)
    remote_gateway_rtr_ids = ",".join(str(r["gateway_rtr_id"]) for r in remotes)
    remote_gateway_ports   = ",".join(str(r["gateway_port"])   for r in remotes)

    for z in range(z_size):
        for y in range(y_size):
            for x in range(x_size):
                rtr_id = (z * x_size * y_size) + (y * x_size) + x
                my_gateway_ports = [r["gateway_port"] for r in remotes if r["gateway_rtr_id"] == rtr_id]
                num_ports = 6 + 1 + len(my_gateway_ports)  # 6 wraparound, local[, gateway...]

                rtr = sst.Component("%s_rtr_%d_%d_%d" % (torus_name, x, y, z), "mordred.mordred_router")
                rtr.addParam("id", rtr_id)
                rtr.addParams({
                    "verbose"         : MINV,
                    "clock"           : clk,
                    "num_vcs"         : num_vcs,
                    "num_vns"         : num_vns,
                    "flit_size"       : flit_size,
                    "input_buf_size"  : UnitAlgebra(16) * flit_size,
                    "output_buf_size" : UnitAlgebra(1)  * flit_size,
                    "num_ports"       : num_ports,
                    "num_local_ports" : 1,
                })
                routers[(x, y, z)] = rtr

                gw_topo = rtr.setSubComponent("topology", "mordred.GatewayTopology")
                gw_topo.addParams({
                    "verbose"                : MINV,
                    "id_base"                : id_base,
                    "local_range_size"       : local_range_size,
                    "remote_id_bases"        : remote_id_bases,
                    "remote_range_sizes"     : remote_range_sizes,
                    "remote_gateway_rtr_ids" : remote_gateway_rtr_ids,
                    "remote_gateway_ports"   : remote_gateway_ports,
                })

                inner_topo = gw_topo.setSubComponent("inner_topology", "mordred.torus3DTopo")
                inner_topo.addParams({"verbose": MINV, "xDim": x_size, "yDim": y_size, "zDim": z_size})

                # ---- test_nic on the local port (plain mordredNIC, no UCIe) ----
                global_id = id_base + rtr_id
                ep_name = "testnic_%s_%d_%d_%d" % (torus_name, x, y, z)
                ep = sst.Component(ep_name, "merlin.test_nic")
                ep.addParams(FixedTestNicParams)
                ep.addParams({"id": global_id, "num_peers": 2 * local_range_size})
                ep_iface = ep.setSubComponent("networkIF", "mordred.mordredNIC")
                ep_iface.addParams(MordredNICParams)

                ep_link = getLink("%s_rtr_%d_%d_%d" % (torus_name, x, y, z), ep_name)
                rtr.addLink(ep_link, "port6", noc_link_latency)
                ep_iface.addLink(ep_link, "port", noc_link_latency)

                # ---- gateway port(s) (extra, additive -- never a repurposed wraparound port) ----
                for i, gw_port in enumerate(sorted(my_gateway_ports)):
                    pc = rtr.setSubComponent("portcontrol", "mordred.rtrPortControlPC", gw_port)
                    pc.addParams(PortControlPCParams)
                    pif = pc.setSubComponent("port_iface", "prydwen.uciePhysChannel", 0)
                    pif.addParams(UCIeParams)
                    pif.addParams({"port_name": "port%d" % gw_port, "endpoint_id": id_base + 100 + i})

    # ---- intra-torus router-router links (plain, full wraparound on all 3 dims) ----
    for z in range(z_size):
        for y in range(y_size):
            for x in range(x_size):
                rtr = routers[(x, y, z)]

                # North (port0): y -> y+1, wrapping to y=0
                if y != y_size - 1:
                    link = getLink("%s_rtr_%d_%d_%d" % (torus_name, x, y, z), "%s_rtr_%d_%d_%d" % (torus_name, x, y + 1, z))
                else:
                    link = getLink("%s_rtr_%d_%d_%d" % (torus_name, x, y, z), "%s_rtr_%d_%d_%d" % (torus_name, x, 0, z))
                rtr.addLink(link, "port0", noc_link_latency)

                # East (port1): x -> x+1, wrapping to x=0
                if x != x_size - 1:
                    link = getLink("%s_rtr_%d_%d_%d" % (torus_name, x, y, z), "%s_rtr_%d_%d_%d" % (torus_name, x + 1, y, z))
                else:
                    link = getLink("%s_rtr_%d_%d_%d" % (torus_name, x, y, z), "%s_rtr_%d_%d_%d" % (torus_name, 0, y, z))
                rtr.addLink(link, "port1", noc_link_latency)

                # South (port2): reverse of north
                if y != 0:
                    link = getLink("%s_rtr_%d_%d_%d" % (torus_name, x, y - 1, z), "%s_rtr_%d_%d_%d" % (torus_name, x, y, z))
                else:
                    link = getLink("%s_rtr_%d_%d_%d" % (torus_name, x, y_size - 1, z), "%s_rtr_%d_%d_%d" % (torus_name, x, 0, z))
                rtr.addLink(link, "port2", noc_link_latency)

                # West (port3): reverse of east
                if x != 0:
                    link = getLink("%s_rtr_%d_%d_%d" % (torus_name, x - 1, y, z), "%s_rtr_%d_%d_%d" % (torus_name, x, y, z))
                else:
                    link = getLink("%s_rtr_%d_%d_%d" % (torus_name, x_size - 1, y, z), "%s_rtr_%d_%d_%d" % (torus_name, 0, y, z))
                rtr.addLink(link, "port3", noc_link_latency)

                # plusZ (port4): z -> z+1, wrapping to z=0
                if z != z_size - 1:
                    link = getLink("%s_rtr_%d_%d_%d" % (torus_name, x, y, z), "%s_rtr_%d_%d_%d" % (torus_name, x, y, z + 1))
                else:
                    link = getLink("%s_rtr_%d_%d_%d" % (torus_name, x, y, z), "%s_rtr_%d_%d_%d" % (torus_name, x, y, 0))
                rtr.addLink(link, "port4", noc_link_latency)

                # minusZ (port5): reverse of plusZ
                if z != 0:
                    link = getLink("%s_rtr_%d_%d_%d" % (torus_name, x, y, z - 1), "%s_rtr_%d_%d_%d" % (torus_name, x, y, z))
                else:
                    link = getLink("%s_rtr_%d_%d_%d" % (torus_name, x, y, z_size - 1), "%s_rtr_%d_%d_%d" % (torus_name, x, y, 0))
                rtr.addLink(link, "port5", noc_link_latency)

    return routers

GATEWAY_PORT_IDX = 7  # 8th port (index 7): 6 wraparound (0-5) + 1 local (6) + 1 gateway (7)

torusA = build_torus("torusA", id_base=0,
                      remotes=[{"id_base": local_range_size, "range_size": local_range_size, "gateway_rtr_id": 0, "gateway_port": GATEWAY_PORT_IDX}])
torusB = build_torus("torusB", id_base=local_range_size,
                      remotes=[{"id_base": 0, "range_size": local_range_size, "gateway_rtr_id": 0, "gateway_port": GATEWAY_PORT_IDX}])

# ---- The single cross-torus UCIe link ----
gateway_link = sst.Link("link_gateway_torusA_torusB")
torusA[(0, 0, 0)].addLink(gateway_link, "port%d" % GATEWAY_PORT_IDX, ucie_link_latency)
torusB[(0, 0, 0)].addLink(gateway_link, "port%d" % GATEWAY_PORT_IDX, ucie_link_latency)

# ---- Statistics ----

sst.setStatisticLoadLevel(7)
sst.setStatisticOutput("sst.statOutputCSV",
    {"filepath": "./stats.%s.csv" % testname, "separator": ", "})
sst.enableAllStatisticsForAllComponents({"rate": "0ns"})

# EOF
