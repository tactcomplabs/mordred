# mesh2x2_gateway_untimed_broadcast.py
#
# Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
# See LICENSE in the top level directory for licensing details
#
# Variant of mesh2x2_gateway_ucie_testnic.py with send_untimed_broadcast=true.
#
# This is the test that exercises GatewayTopology::routeUntimedBroadcastPacket()
# -- specifically, that an untimed broadcast originating in one domain (mesh
# A or mesh B) gets relayed across the single UCIe gateway link and floods
# the OTHER domain too, not just the domain it originated in.
#
# merlin.test_nic sends broadcast packets during SST's init/complete phases
# when send_untimed_broadcast=true, and every test_nic waits
# (init_broadcast_count == num_peers-1) before its own init completes -- so
# if the gateway relay were missing or wrong, this test would not crash, it
# would simply HANG until ctest's timeout, rather than ever reaching
# "Simulation is complete". That's the same failure mode
# mesh3x3_untimed_broadcast.py relies on for MeshTopology's own (single-
# domain) flood, applied here across the gateway.
#
# See GatewayTopology.h / mesh2x2_gateway_ucie_testnic.py for the full
# two-mesh topology and gateway design; only send_untimed_broadcast changes
# here.
#

import sst
from sst import UnitAlgebra

testname = "mesh2x2_gateway_untimed_broadcast"

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
    "send_untimed_broadcast" : "true",   # <-- the only real change from mesh2x2_gateway_ucie_testnic.py
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

links = dict()
def getLink(name1, name2):
    name = "link.%s_%s" % (name1, name2)
    if name not in links:
        links[name] = sst.Link(name)
    return links[name]

def build_mesh(mesh_name, id_base, gateway_rtr_id, gateway_port_idx):
    """Build one self-contained 2x2 mesh; returns its routers keyed by (x,y).

    mesh_name        -- prefix for component names (e.g. "meshA")
    id_base          -- this mesh's offset into the shared global id space
    gateway_rtr_id   -- LOCAL rtr_id (0..3) of this mesh's designated gateway router
    gateway_port_idx -- port index the gateway router uses for the cross-mesh link
    """
    routers = {}

    for y in range(y_size):
        for x in range(x_size):
            rtr_id = y * x_size + x
            is_gateway = (rtr_id == gateway_rtr_id)
            num_ports = (4 + 1 + 1) if is_gateway else (4 + 1)  # N,E,S,W,local[,gateway]

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
            gw_params = {
                "verbose"          : MAXV,
                "id_base"          : id_base,
                "gateway_rtr_id"   : gateway_rtr_id,
                "local_range_size" : local_range_size,
            }
            if is_gateway:
                gw_params["gateway_port"] = gateway_port_idx
            gw_topo.addParams(gw_params)

            inner_topo = gw_topo.setSubComponent("inner_topology", "mordred.MeshTopology")
            inner_topo.addParams({"verbose": MAXV, "xDim": x_size, "yDim": y_size})

            # ---- test_nic on the local port (plain mordredNIC, no UCIe) ----
            global_id = id_base + rtr_id
            ep_name = "testnic_%s_%d_%d" % (mesh_name, x, y)
            ep = sst.Component(ep_name, "merlin.test_nic")
            ep.addParams(FixedTestNicParams)
            ep.addParams({"id": global_id, "num_peers": 2 * local_range_size})
            ep_iface = ep.setSubComponent("networkIF", "mordred.mordredNIC")
            ep_iface.addParams(MordredNICParams)

            ep_link = getLink("%s_rtr_%d_%d" % (mesh_name, x, y), ep_name)
            rtr.addLink(ep_link, "port4", noc_link_latency)
            ep_iface.addLink(ep_link, "port", noc_link_latency)

            # ---- gateway port (extra, additive -- never a repurposed cardinal port) ----
            if is_gateway:
                pc = rtr.setSubComponent("portcontrol", "mordred.rtrPortControlPC", gateway_port_idx)
                pc.addParams(PortControlPCParams)
                pif = pc.setSubComponent("port_iface", "prydwen.uciePhysChannel", 0)
                pif.addParams(UCIeParams)
                pif.addParams({"port_name": "port%d" % gateway_port_idx, "endpoint_id": id_base + 100})

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

GATEWAY_PORT_IDX = 5  # 6th port (index 5): 4 cardinal (0-3) + 1 local (4) + 1 gateway (5)

meshA = build_mesh("meshA", id_base=0, gateway_rtr_id=1, gateway_port_idx=GATEWAY_PORT_IDX)
meshB = build_mesh("meshB", id_base=4, gateway_rtr_id=0, gateway_port_idx=GATEWAY_PORT_IDX)

# ---- The single cross-mesh UCIe link ----
gateway_link = sst.Link("link_gateway_meshA_meshB")
meshA[(1, 0)].addLink(gateway_link, "port%d" % GATEWAY_PORT_IDX, ucie_link_latency)
meshB[(0, 0)].addLink(gateway_link, "port%d" % GATEWAY_PORT_IDX, ucie_link_latency)

# ---- Statistics ----

sst.setStatisticLoadLevel(7)
sst.setStatisticOutput("sst.statOutputCSV",
    {"filepath": "./stats.%s.csv" % testname, "separator": ", "})
sst.enableAllStatisticsForAllComponents({"rate": "0ns"})

# EOF
