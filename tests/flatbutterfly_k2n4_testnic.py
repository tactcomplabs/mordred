# Automatically generated SST Python input

import sst
from math import floor

# Use to set the stats output filename
testname = "flatbutterfly_k2n4"

## TEST NOTE(S)
## - Additional combinations of k,n are included here for reference

FixedRtrParams = {
    "verbose" : "0",
    "clock" : "1GHz",
    "num_vcs" : "1",
    "flit_size" : "16b",
    "input_buf_size" : "32B", # 16 flits
    "output_buf_size" : "16b" # 1 flit
}

FixedTestNicParams = {
    "num_messages" : 10,
    "message_size" : "8B",
    "send_untimed_broadcast" : "false", # matches default
}

MordredNICParams = {
    "verbose" : 0,
    "input_buf_size" : "1kiB",
    "output_buf_size" : "1kiB",
}

links = dict()
def getLink(name1, name2):
    name = "link.%s_%s"%(name1, name2)
    if name not in links:
        links[name] = sst.Link(name)
        #print("New link: %s"%name)
    return links[name]

rtr_port_nums = dict()
def getNextTopoPort(name):
    if name not in rtr_port_nums:
        rtr_port_nums[name] = 1
    else:
        rtr_port_nums[name] += 1
    return (rtr_port_nums[name] - 1)


class FlattenedButterfly:
    def __init__(self, k, n):
        self.k = k
        self.n = n
        self.num_endpoints = pow(k, n)
        self.num_routers = floor(self.num_endpoints/k)
        self.radix = ( n * (k-1) ) + 1
        self.local_port_start = self.radix - self.k
        self.num_dims = n - 1
        self.flatfly_links = dict()
        self.routers = self.gen_routers()
        self.create_topo_links()
        self.endpoints = self.gen_endpoints()

    def gen_routers(self):

        routers = []
        for i in range(self.num_routers):
            rtr_name = "rtr_%d"%(i)
            routers.append(sst.Component(rtr_name, "mordred.simple_rtr"))
            routers[i].addParam( "id", i )
            routers[i].addParams(FixedRtrParams)
            routers[i].addParam( "num_ports", self.radix )
            routers[i].addParam( "num_local_ports", self.k )
            rtr_topo = routers[i].setSubComponent( "topology", "mordred.flattenedButterfly" )
            rtr_topo.addParams({
                "verbose" : 0,
                "k" : self.k,
                "n" : self.n,
            })
        return routers

    # The range of d might be off a hair - paper has it as 1, self.n-1 but that blows up
    # on a 4-ary, 2-fly.
    def create_topo_links(self):
        for d in range(self.num_dims):
            for m in range (self.k-1):
                for i in range(self.num_routers):
                    ff = floor(i/pow(self.k,d)) % self.k
                    j = i + ( ( m - ff ) * pow(self.k,d) )
                    if ( j != i ):
                        self.create_link(i,j)

    def create_link(self, i, j):
        if (i > j):
            i,j = j,i
        link_name, new_link = self.getFlatFlyLink("rtr_%d"%(i), "rtr_%d"%(j))
        if new_link:
            rtr_pname = "port" + str(getNextTopoPort("rtr_%d"%i))
            self.routers[i].addLink(link_name, rtr_pname, "800ps")
            rtr_pname = "port" + str(getNextTopoPort("rtr_%d"%j))
            self.routers[j].addLink(link_name, rtr_pname, "800ps")


    def getFlatFlyLink(self, name1, name2):
        name = "link.%s_%s"%(name1, name2)
        if name not in self.flatfly_links:
            self.flatfly_links[name] = sst.Link(name)
            return self.flatfly_links[name], True
        return self.flatfly_links[name], False

    def gen_endpoints(self):
        endpoints = []
        for i in range(self.num_routers):
            for j in range(self.k):
                portname = "port" + str(self.local_port_start + j)
                ep_name = "ep_%d_%d"%(i,j)
                ep_num = i*self.k + j
                ep = sst.Component(ep_name, "merlin.test_nic")
                ep.addParams(FixedTestNicParams)
                ep.addParams({
                    "id" : ep_num,
                    "num_peers" : self.num_endpoints,
                })

                endpoints.append(ep)
                ep_iface = ep.setSubComponent( "networkIF", "mordred.mordredNIC" )
                ep_iface.addParams(MordredNICParams)

                rtr_id = "rtr_%d"%i
                ep_id = "ep_%d"%j
                link_name, new_link = self.createEpRtrLink(rtr_id, ep_id)
                if not new_link:
                    print("WARN Failed to create ep link")
                self.routers[i].addLink(link_name, portname, "800ps")
                ep_iface.addLink(link_name, "port", "800ps")

    def createEpRtrLink(self, rtr_id, ep_id):
        name = "link.%s_%s"%(rtr_id, ep_id)
        if name not in self.flatfly_links:
            self.flatfly_links[name] = sst.Link(name)
            return self.flatfly_links[name], True
        return self.flatfly_links[name], False

# General params
local_ports = 1 # == concentration

# Flattened Butterfly Paper
# Flattened Butterfly : A Cost-Efficient Topology for
# High-Radix Networks, ISCA 2007

#print("Fig 1D in flat fly paper")
flatfly = FlattenedButterfly(2, 4) # fig 1d in paper; 16 endpoints

#print("Fig 1B in flat fly paper")
#flatfly2 = FlattenedButterfly(4, 2) # fig 1b in paper; 16 endpoints

#print("Fig 3 in Micro2007 FlatFly Paper")
#flatfly3 = FlattenedButterfly(4, 3) # 64 endpoints

# Do stats
sst.setStatisticLoadLevel(7)
stat_params = ( { "rate" : "0ns" } )
sst.setStatisticOutput("sst.statOutputCSV", { "filepath" : "./stats.%s.csv"%testname, "separator" : ", " } )
sst.enableAllStatisticsForAllComponents(stat_params)

#EOF
