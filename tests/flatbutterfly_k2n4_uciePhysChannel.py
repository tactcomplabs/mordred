# flatbutterfly_k2n4_uciePhysChannel.py
#
# Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
# See LICENSE in the top level directory for licensing details
#
# uciePhysChannel variant of flatbutterfly_k2n4_testnic.py.
#
# Every link in the flattened-butterfly network uses prydwen.uciePhysChannel
# as the physical transport.  The topology parameters (k=2, n=4, 16 endpoints)
# and traffic parameters are identical to the original.
#

import sst
from sst import UnitAlgebra
from math import floor

testname = "flatbutterfly_k2n4_uciePhysChannel"

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
    "num_vcs"         : "1",
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

# ---- port-index tracker (mirrors getNextTopoPort in original) ----

rtr_port_nums = dict()

def getNextTopoPort(name):
    if name not in rtr_port_nums:
        rtr_port_nums[name] = 1
    else:
        rtr_port_nums[name] += 1
    return rtr_port_nums[name] - 1


def wire_rtr_port(rtr, port_idx, port_name, link, ucie_ep_id):
    """Attach rtrPortControlPC + UCIePhysChannel to one router port and connect the link."""
    pc = rtr.setSubComponent("portcontrol", "mordred.rtrPortControlPC", port_idx)
    pc.addParams(PortControlPCParams)
    ucie = pc.setSubComponent("port_iface", "prydwen.uciePhysChannel", 0)
    ucie.addParams(UCIeParams)
    ucie.addParams({"port_name": port_name, "endpoint_id": ucie_ep_id})
    rtr.addLink(link, port_name, link_latency)


class FlattenedButterfly:
    def __init__(self, k, n):
        self.k = k
        self.n = n
        self.num_endpoints  = pow(k, n)
        self.num_routers    = floor(self.num_endpoints / k)
        self.radix          = (n * (k - 1)) + 1
        self.local_port_start = self.radix - self.k
        self.num_dims       = n - 1
        self.flatfly_links  = dict()
        self.routers        = self.gen_routers()
        self.create_topo_links()
        self.endpoints      = self.gen_endpoints()

    def gen_routers(self):
        routers = []
        for i in range(self.num_routers):
            rtr_name = "rtr_%d" % i
            routers.append(sst.Component(rtr_name, "mordred.mordred_router"))
            routers[i].addParam("id", i)
            routers[i].addParams(FixedRtrParams)
            routers[i].addParam("num_ports", self.radix)
            routers[i].addParam("num_local_ports", self.k)
            rtr_topo = routers[i].setSubComponent("topology", "mordred.flattenedButterfly")
            rtr_topo.addParams({"verbose": 0, "k": self.k, "n": self.n})
        return routers

    def create_topo_links(self):
        for d in range(self.num_dims):
            for m in range(self.k - 1):
                for i in range(self.num_routers):
                    ff = floor(i / pow(self.k, d)) % self.k
                    j  = i + ((m - ff) * pow(self.k, d))
                    if j != i:
                        self.create_link(i, j)

    def create_link(self, i, j):
        if i > j:
            i, j = j, i
        link_obj, new_link = self.getFlatFlyLink("rtr_%d" % i, "rtr_%d" % j)
        if new_link:
            port_i   = getNextTopoPort("rtr_%d" % i)
            pname_i  = "port%d" % port_i
            wire_rtr_port(self.routers[i], port_i, pname_i, link_obj,
                          3000 + i * 100 + port_i)

            port_j   = getNextTopoPort("rtr_%d" % j)
            pname_j  = "port%d" % port_j
            wire_rtr_port(self.routers[j], port_j, pname_j, link_obj,
                          3000 + j * 100 + port_j)

    def getFlatFlyLink(self, name1, name2):
        name = "link.%s_%s" % (name1, name2)
        if name not in self.flatfly_links:
            self.flatfly_links[name] = sst.Link(name)
            return self.flatfly_links[name], True
        return self.flatfly_links[name], False

    def gen_endpoints(self):
        endpoints = []
        for i in range(self.num_routers):
            for j in range(self.k):
                port_idx  = self.local_port_start + j
                port_name = "port%d" % port_idx
                ep_name   = "ep_%d_%d" % (i, j)
                ep_num    = i * self.k + j

                ep = sst.Component(ep_name, "merlin.test_nic")
                ep.addParams(FixedTestNicParams)
                ep.addParams({"id": ep_num, "num_peers": self.num_endpoints})
                endpoints.append(ep)

                link_obj, new_link = self.createEpRtrLink("rtr_%d" % i, "ep_%d" % j)
                if not new_link:
                    print("WARN Failed to create ep link")

                # Router-side local port
                wire_rtr_port(self.routers[i], port_idx, port_name, link_obj,
                              4000 + i * 10 + j)

                # NIC-side
                ep_iface = ep.setSubComponent("networkIF", "mordred.mordredNicPC")
                ep_iface.addParams(MordredNicPCParams)
                ucie_ep = ep_iface.setSubComponent("port_iface", "prydwen.uciePhysChannel", 0)
                ucie_ep.addParams(UCIeParams)
                ucie_ep.addParams({"port_name": "port", "endpoint_id": ep_num})
                ep_iface.addLink(link_obj, "port", link_latency)

    def createEpRtrLink(self, rtr_id, ep_id):
        name = "link.%s_%s" % (rtr_id, ep_id)
        if name not in self.flatfly_links:
            self.flatfly_links[name] = sst.Link(name)
            return self.flatfly_links[name], True
        return self.flatfly_links[name], False


flatfly = FlattenedButterfly(2, 4)  # 16 endpoints, k=2 n=4 (fig 1d)

sst.setStatisticLoadLevel(7)
sst.setStatisticOutput("sst.statOutputCSV",
    {"filepath": "./stats.%s.csv" % testname, "separator": ", "})
sst.enableAllStatisticsForComponentType("prydwen.uciePhysChannel")

# EOF
