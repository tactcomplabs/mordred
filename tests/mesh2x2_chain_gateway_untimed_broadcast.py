# mesh2x2_chain_gateway_untimed_broadcast.py
#
# Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
# See LICENSE in the top level directory for licensing details
#
# Variant of mesh2x2_chain_gateway_ucie_testnic.py with
# send_untimed_broadcast=true.
#
# This is the test that exercises GatewayTopology's MULTI-HOP broadcast
# relay: a broadcast originating in mesh A must cross the A-B link, flood
# mesh B, and then continue across the B-C link to flood mesh C too -- there
# is no direct A-C link, so this only works if a broadcast arriving at B
# from A actually gets relayed onward out B's OTHER gateway (to C), not just
# absorbed into B's own domain. See GatewayTopology.h's class doc comment
# and GatewayTopology.cc's routeUntimedBroadcastPacket() for the mechanism.
#
# Same completion criterion as mesh2x2_gateway_untimed_broadcast.py: every
# test_nic's init handshake requires init_broadcast_count to reach EXACTLY
# num_peers-1 (a strict equality check in nic.cc) before it can proceed past
# its init state machine. If mesh C's broadcasts never reached mesh A (or
# vice versa), the affected nodes would simply never satisfy that check and
# the simulation would hang until ctest's timeout, rather than ever
# reaching "Simulation is complete" -- so a clean completion here is a real,
# rigorous confirmation that every one of the 12 nodes' broadcasts reached
# all 11 others exactly once, across two hops.
#
# See mesh2x2_chain_gateway_ucie_testnic.py for the full topology; only
# send_untimed_broadcast changes here.
#

import sst
from sst import UnitAlgebra

testname = "mesh2x2_chain_gateway_untimed_broadcast"

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
    "credits_per_vn"    : "64",
    "flit_format"       : 5,
    "num_modules"       : 1,
    "num_lanes"         : 16,
    "lane_speed_gts"    : 32,
    "verbose"           : MAXV,
}

PortControlPCParams = {
    "flit_size"       : flit_size,
    "input_buf_size"  : UnitAlgebra(32) * flit_size,
    "output_buf_size" : UnitAlgebra(2)  * flit_size,
    "verbose"         : MAXV,
}

FixedTestNicParams = {
    "num_messages"           : 2,
    "message_size"           : UnitAlgebra(4) * flit_size,
    "send_untimed_broadcast" : "true",   # <-- the only real change from mesh2x2_chain_gateway_ucie_testnic.py
}

MordredNICParams = {
    "verbose"         : MAXV,
    "clock"           : clk,
    "input_buf_size"  : "1kiB",
    "output_buf_size" : "1kiB",
}

# ---- Mesh configuration ----
x_size = 2
y_size = 2
local_range_size = x_size * y_size  # 1 local port per router -> 4 endpoints/mesh
num_domains = 3

links = dict()
def getLink(name1, name2):
    name = "link.%s_%s" % (name1, name2)
    if name not in links:
        links[name] = sst.Link(name)
    return links[name]

def build_mesh(mesh_name, id_base, remotes):
    """Build one self-contained 2x2 mesh; returns its routers keyed by (x,y).

    mesh_name -- prefix for component names (e.g. "meshA")
    id_base   -- this mesh's offset into the shared global id space
    remotes   -- list of dicts, one per remote RANGE this mesh knows how to
                 reach (not necessarily one per remote MESH -- a leaf in a
                 chain names two ranges, both via its one physical gateway):
                 {"id_base":, "range_size":, "gateway_rtr_id":, "gateway_port":}
                 gateway_rtr_id is the LOCAL rtr_id (0..3) of THIS mesh's
                 gateway router for that range; entries that share the same
                 (gateway_rtr_id, gateway_port) collapse to one physical port.
    """
    routers = {}
    remote_id_bases        = ",".join(str(r["id_base"])        for r in remotes)
    remote_range_sizes     = ",".join(str(r["range_size"])     for r in remotes)
    remote_gateway_rtr_ids = ",".join(str(r["gateway_rtr_id"]) for r in remotes)
    remote_gateway_ports   = ",".join(str(r["gateway_port"])   for r in remotes)

    for y in range(y_size):
        for x in range(x_size):
            rtr_id = y * x_size + x
            my_gateway_ports = sorted(set(r["gateway_port"] for r in remotes if r["gateway_rtr_id"] == rtr_id))
            num_ports = 4 + 1 + len(my_gateway_ports)  # N,E,S,W,local[,gateway...]

            rtr = sst.Component("%s_rtr_%d_%d" % (mesh_name, x, y), "mordred.mordred_router")
            rtr.addParam("id", rtr_id)
            rtr.addParams({
                "verbose"         : MAXV,
                "clock"           : clk,
                "num_vcs"         : num_vcs,
                "num_vns"         : num_vns,
                "flit_size"       : flit_size,
                "input_buf_size"  : UnitAlgebra(16) * flit_size,
                "output_buf_size" : UnitAlgebra(1)  * flit_size,
                "num_ports"       : num_ports,
                "num_local_ports" : 1,
            })
            routers[(x, y)] = rtr

            gw_topo = rtr.setSubComponent("topology", "mordred.GatewayTopology")
            gw_topo.addParams({
                "verbose"                : MAXV,
                "id_base"                : id_base,
                "local_range_size"       : local_range_size,
                "remote_id_bases"        : remote_id_bases,
                "remote_range_sizes"     : remote_range_sizes,
                "remote_gateway_rtr_ids" : remote_gateway_rtr_ids,
                "remote_gateway_ports"   : remote_gateway_ports,
            })

            inner_topo = gw_topo.setSubComponent("inner_topology", "mordred.MeshTopology")
            inner_topo.addParams({"verbose": MAXV, "xDim": x_size, "yDim": y_size})

            # ---- test_nic on the local port (plain mordredNIC, no UCIe) ----
            global_id = id_base + rtr_id
            ep_name = "testnic_%s_%d_%d" % (mesh_name, x, y)
            ep = sst.Component(ep_name, "merlin.test_nic")
            ep.addParams(FixedTestNicParams)
            ep.addParams({"id": global_id, "num_peers": num_domains * local_range_size})
            ep_iface = ep.setSubComponent("networkIF", "mordred.mordredNIC")
            ep_iface.addParams(MordredNICParams)

            ep_link = getLink("%s_rtr_%d_%d" % (mesh_name, x, y), ep_name)
            rtr.addLink(ep_link, "port4", noc_link_latency)
            ep_iface.addLink(ep_link, "port", noc_link_latency)

            # ---- gateway port(s) (extra, additive -- never a repurposed cardinal port) ----
            for i, gw_port in enumerate(my_gateway_ports):
                pc = rtr.setSubComponent("portcontrol", "mordred.rtrPortControlPC", gw_port)
                pc.addParams(PortControlPCParams)
                pif = pc.setSubComponent("port_iface", "prydwen.uciePhysChannel", 0)
                pif.addParams(UCIeParams)
                pif.addParams({"port_name": "port%d" % gw_port, "endpoint_id": id_base + 100 + i})

    # ---- intra-mesh router-router links (plain, standard 2x2 mesh) ----
    for y in range(y_size):
        for x in range(x_size):
            rtr = routers[(x, y)]

            # North (port0)
            if y != y_size - 1:
                link = getLink("%s_rtr_%d_%d" % (mesh_name, x, y), "%s_rtr_%d_%d" % (mesh_name, x, y + 1))
                rtr.addLink(link, "port0", noc_link_latency)

            # East (port1)
            if x != x_size - 1:
                link = getLink("%s_rtr_%d_%d" % (mesh_name, x, y), "%s_rtr_%d_%d" % (mesh_name, x + 1, y))
                rtr.addLink(link, "port1", noc_link_latency)

            # South (port2)
            if y != 0:
                link = getLink("%s_rtr_%d_%d" % (mesh_name, x, y - 1), "%s_rtr_%d_%d" % (mesh_name, x, y))
                rtr.addLink(link, "port2", noc_link_latency)

            # West (port3)
            if x != 0:
                link = getLink("%s_rtr_%d_%d" % (mesh_name, x - 1, y), "%s_rtr_%d_%d" % (mesh_name, x, y))
                rtr.addLink(link, "port3", noc_link_latency)

    return routers

GATEWAY_PORT_IDX = 5  # 6th port (index 5): every gateway router here hosts exactly one physical link

ID_BASE_A, ID_BASE_B, ID_BASE_C = 0, local_range_size, 2 * local_range_size

# A: two ranges (B, and C-via-B), both via the same physical gateway (rtr1 -> B).
meshA = build_mesh("meshA", id_base=ID_BASE_A, remotes=[
    {"id_base": ID_BASE_B, "range_size": local_range_size, "gateway_rtr_id": 1, "gateway_port": GATEWAY_PORT_IDX},  # -> B, direct
    {"id_base": ID_BASE_C, "range_size": local_range_size, "gateway_rtr_id": 1, "gateway_port": GATEWAY_PORT_IDX},  # -> C, via B
])

# B: two ACCURATE, single-hop ranges, each via its own distinct physical gateway.
meshB = build_mesh("meshB", id_base=ID_BASE_B, remotes=[
    {"id_base": ID_BASE_A, "range_size": local_range_size, "gateway_rtr_id": 0, "gateway_port": GATEWAY_PORT_IDX},  # -> A, direct
    {"id_base": ID_BASE_C, "range_size": local_range_size, "gateway_rtr_id": 1, "gateway_port": GATEWAY_PORT_IDX},  # -> C, direct
])

# C: mirrors A -- two ranges (B, and A-via-B), both via the same physical gateway (rtr0 -> B).
meshC = build_mesh("meshC", id_base=ID_BASE_C, remotes=[
    {"id_base": ID_BASE_B, "range_size": local_range_size, "gateway_rtr_id": 0, "gateway_port": GATEWAY_PORT_IDX},  # -> B, direct
    {"id_base": ID_BASE_A, "range_size": local_range_size, "gateway_rtr_id": 0, "gateway_port": GATEWAY_PORT_IDX},  # -> A, via B
])

# ---- The two physical UCIe links (A-B and B-C -- there is no A-C link) ----

link_ab = sst.Link("link_gateway_meshA_meshB")
meshA[(1, 0)].addLink(link_ab, "port%d" % GATEWAY_PORT_IDX, ucie_link_latency)
meshB[(0, 0)].addLink(link_ab, "port%d" % GATEWAY_PORT_IDX, ucie_link_latency)

link_bc = sst.Link("link_gateway_meshB_meshC")
meshB[(1, 0)].addLink(link_bc, "port%d" % GATEWAY_PORT_IDX, ucie_link_latency)
meshC[(0, 0)].addLink(link_bc, "port%d" % GATEWAY_PORT_IDX, ucie_link_latency)

# ---- Statistics ----

sst.setStatisticLoadLevel(7)
sst.setStatisticOutput("sst.statOutputCSV",
    {"filepath": "./stats.%s.csv" % testname, "separator": ", "})
sst.enableAllStatisticsForAllComponents({"rate": "0ns"})

# EOF
