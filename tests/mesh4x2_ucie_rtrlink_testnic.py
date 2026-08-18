# mesh4x2_ucie_rtrlink_testnic.py
#
# Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
# See LICENSE in the top level directory for licensing details
#
# Builds on mesh2x1_ucie_rtrlink_testnic.py: instead of a single router
# (with one test_nic) on each side of the UCIe link, each side is now a
# full 2x2 mesh (4 routers, 4 test_nics). As before, every test_nic-to-
# router hop is a plain mordred.mordredNIC link -- no UCIe/PhysChannelAPI
# anywhere except the one designated router-router link.
#
# IMPORTANT CAVEAT (why this isn't two independent 2x2 fabrics):
# Mordred's MeshTopology computes routing from a single, globally shared
# (xDim, yDim) -- see MeshTopology::routePacket() in MeshTopology.cc,
# which derives dest_x/dest_y from "dest / numLocalPorts" using the same
# xDim for every router. There is no address-translation/bridge layer
# (unlike memHierarchy's merlin.Bridge, used in mordred_testBridge.py),
# so two *independently addressed* 2x2 meshes cannot auto-route traffic
# across a UCIe link -- the whole 8-router fabric has to be ONE unified
# 4x2 mesh (two 2x2 halves = columns 0-1 and columns 2-3) for the two
# halves to exchange any real endpoint traffic at all.
#
# A rectangular mesh split down the middle of a 2-wide block necessarily
# has TWO parallel boundary links (one per row). merlin.test_nic has no
# way to restrict which peers it talks to -- nic.cc's clock_handler()
# always cycles dest=0..num_peers-1 -- so with all 8 test_nics addressing
# each other (num_peers=8), dimension-order routing requires BOTH
# boundary links to physically exist, or routing for the unlinked row
# would be sent into a nonexistent port. So exactly ONE of the two
# boundary links (rtr_1_0 <-> rtr_2_0, row 0) is the UCIe link under
# test; the other (rtr_1_1 <-> rtr_2_1, row 1) is an ordinary link,
# required for full-mesh correctness, not part of what's being exercised.
# Traffic that originates in row 0 and crosses halves uses the UCIe link;
# traffic that originates in row 1 and crosses halves uses the plain
# link instead -- not all inter-half traffic is guaranteed to use UCIe,
# but a real (and majority, by node count) share of it does.
#
# Topology (5 ports/router: port0=N, port1=E, port2=S, port3=W, port4=
# local; router ids are row-major, x=id%4, y=id/4):
#
#   rtr_0_0 -- rtr_1_0 == UCIe == rtr_2_0 -- rtr_3_0      (row 0, y=0)
#      |          |                  |          |
#   rtr_0_1 -- rtr_1_1 ---plain---- rtr_2_1 -- rtr_3_1    (row 1, y=1)
#
# Every router has exactly one merlin.test_nic on its local port via
# plain mordred.mordredNIC (no portcontrol subcomponent on that hop --
# the router falls back to an anonymous mordred.rtrPortControl there,
# same as mesh3x3_testnic.py). All components run at max verbosity, each
# test_nic sends only 2 messages to each of the 8 peers, and untimed
# broadcasts are disabled.
#

import sst
from sst import UnitAlgebra

testname = "mesh4x2_ucie_rtrlink_testnic"

# ---- Simulation parameters ----
MAXV              = 10  # MORDRED_VERBOSE_ALL (see MordredEvents.h)
clk               = UnitAlgebra("1GHz")
clk_pd            = clk.invert()
noc_link_latency  = UnitAlgebra(0.8) * clk_pd
flit_size         = UnitAlgebra("16b")
num_vns           = 1
num_vcs           = 1
ucie_link_latency = "2ns"

UCIeParams = {
    "link_latency"      : ucie_link_latency,
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

# ---- Mesh configuration ----
x_size = 4
y_size = 2
num_eps = x_size * y_size  # 1 local port per router

# The single UCIe boundary edge joining the two 2x2 halves (row 0).
UCIE_EDGE = ((1, 0), (2, 0))

links = dict()
def getLink(name1, name2):
    name = "link.%s_%s" % (name1, name2)
    if name not in links:
        links[name] = sst.Link(name)
    return links[name]

next_ucie_epid = [100]
def wire_ucie_edge(rtr_a, port_a, rtr_b, port_b, link):
    """Wire one router-router edge via RtrPortControlPC + prydwen.uciePhysChannel."""
    pc_a = rtr_a.setSubComponent("portcontrol", "mordred.rtrPortControlPC", int(port_a[4:]))
    pc_a.addParams(PortControlPCParams)
    pif_a = pc_a.setSubComponent("port_iface", "prydwen.uciePhysChannel", 0)
    pif_a.addParams(UCIeParams)
    pif_a.addParams({"port_name": port_a, "endpoint_id": next_ucie_epid[0]})
    next_ucie_epid[0] += 1

    pc_b = rtr_b.setSubComponent("portcontrol", "mordred.rtrPortControlPC", int(port_b[4:]))
    pc_b.addParams(PortControlPCParams)
    pif_b = pc_b.setSubComponent("port_iface", "prydwen.uciePhysChannel", 0)
    pif_b.addParams(UCIeParams)
    pif_b.addParams({"port_name": port_b, "endpoint_id": next_ucie_epid[0]})
    next_ucie_epid[0] += 1

    rtr_a.addLink(link, port_a, ucie_link_latency)
    rtr_b.addLink(link, port_b, ucie_link_latency)

# ---- Create routers + test_nics ----

routers = {}

for y in range(y_size):
    for x in range(x_size):
        rtr_id = y * x_size + x
        rtr = sst.Component("rtr_%d_%d" % (x, y), "mordred.mordred_router")
        rtr.addParam("id", rtr_id)
        rtr.addParams(FixedRtrParams)
        rtr_topo = rtr.setSubComponent("topology", "mordred.MeshTopology")
        rtr_topo.addParams({"verbose": MAXV, "xDim": x_size, "yDim": y_size})
        routers[(x, y)] = rtr

        ep_name = "testnic_ep_%d_%d" % (x, y)
        ep = sst.Component(ep_name, "merlin.test_nic")
        ep.addParams(FixedTestNicParams)
        ep.addParams({"id": rtr_id, "num_peers": num_eps})
        ep_iface = ep.setSubComponent("networkIF", "mordred.mordredNIC")
        ep_iface.addParams(MordredNICParams)

        ep_link = getLink("rtr_%d_%d" % (x, y), ep_name)
        rtr.addLink(ep_link, "port4", noc_link_latency)
        ep_iface.addLink(ep_link, "port", noc_link_latency)

# ---- Wire router-router links (plain, except the one UCIe boundary edge) ----

for y in range(y_size):
    for x in range(x_size):
        rtr = routers[(x, y)]

        # North (port0): (x,y) -> (x,y+1)
        if y != y_size - 1:
            link = getLink("rtr_%d_%d" % (x, y), "rtr_%d_%d" % (x, y + 1))
            rtr.addLink(link, "port0", noc_link_latency)

        # East (port1): (x,y) -> (x+1,y)
        if x != x_size - 1:
            link = getLink("rtr_%d_%d" % (x, y), "rtr_%d_%d" % (x + 1, y))
            if ((x, y), (x + 1, y)) == UCIE_EDGE:
                wire_ucie_edge(rtr, "port1", routers[(x + 1, y)], "port3", link)
            else:
                rtr.addLink(link, "port1", noc_link_latency)

        # South (port2): reverse of north
        if y != 0:
            link = getLink("rtr_%d_%d" % (x, y - 1), "rtr_%d_%d" % (x, y))
            rtr.addLink(link, "port2", noc_link_latency)

        # West (port3): reverse of east -- skip the UCIe edge (already wired above)
        if x != 0 and ((x - 1, y), (x, y)) != UCIE_EDGE:
            link = getLink("rtr_%d_%d" % (x - 1, y), "rtr_%d_%d" % (x, y))
            rtr.addLink(link, "port3", noc_link_latency)

# ---- Statistics ----

sst.setStatisticLoadLevel(7)
sst.setStatisticOutput("sst.statOutputCSV",
    {"filepath": "./stats.%s.csv" % testname, "separator": ", "})
sst.enableAllStatisticsForAllComponents({"rate": "0ns"})

# EOF
