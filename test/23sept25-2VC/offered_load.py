# Automatically generated SST Python input
from selectors import SelectSelector

import sst
from math import floor

load_level = 80
load_factor = (load_level/100)

sst.setProgramOption("stop-at", "1ms")

stat_params = ( { "rate" : "0ns" } )
sst.setStatisticOutput("sst.statOutputCSV", { "filepath" : "./mordred.OL.LF%s.csv"%load_level, "separator" : ", " } )

FixedRtrParams = {
    "num_vcs" : "2",
    "flit_size" : "16b",
    "input_buf_size" : "32B",
    "output_buf_size" : "16b"
}

# Will need to fix statmemts using this variable if we add more options
merlin_trafficgen = 0 # set to 0 is merlin.offered_load, 1 is merlin.background_traffic, 2 is merlin.clocked_offered_load

MatchingTrafficGenParams = {
    "packet_size" : "64b",
    "offered_load" : load_factor,
    "pattern" : "merlin.targetgen.uniform"
}

OfferedLoadParams = {
    "link_bw" : "1GB/s",
    "linkcontrol" : "mordred.mordredNIC",
    "buffer_size" : "1kiB",
    "warmup_time" : "1us",
    "collect_time" : "500us",
    "drain_time" : "50us"
}

ClockedOfferedLoadParams = {
    "clock_rate" : "1GHz",
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

# There are local_ports endpoints connected to each router
# rtr_id is expected to go linearly from 0-((x*y)-1)
# rtr_id is also expected to be x-dominant (e.g, router id xDim has location x=0,y=1)
# links are expected to be ordered as n,e,s,w
def createMesh(x_size, y_size, local_ports):
    # Create the routers
    rtr_id = 0
    nports = 4 + local_ports
    rtr_params = {
        "num_ports": nports,
        "num_local_ports" : local_ports,
    }
    for y in range(y_size):
        for x in range(x_size):
            rtr = sst.Component("rtr_%d_%d"%(x, y), "mordred.simple_rtr")
            rtr.addParam("id", rtr_id)
            rtr.addParams(FixedRtrParams)
            rtr.addParams(rtr_params)
            rtr_id += 1
            rtr_topo = rtr.setSubComponent( "topology", "mordred.MeshTopology" )
            rtr_topo.addParams({
                "xDim" : x_size,
                "yDim" : y_size
            })
            rtr_portnum = 0
            rtr_portname = "port" + str(rtr_portnum)
            # north links
            if y != y_size - 1:
                rtr.addLink(getLink("rtr_%d_%d"%(x,y), "rtr_%d_%d"%(x,y+1)), rtr_portname, "800ps")
                #print("Add north link with portname=%s to x,y=%d_%d"%(rtr_portname,x,y))
            rtr_portnum += 1
            rtr_portname = "port" + str(rtr_portnum)

            # east links
            if x != x_size - 1:
                rtr.addLink(getLink("rtr_%d_%d"%(x,y), "rtr_%d_%d"%(x+1,y)), rtr_portname, "800ps")
                #print("Add east link with portname=%s to x,y=%d_%d"%(rtr_portname,x,y))
            rtr_portnum += 1
            rtr_portname = "port" + str(rtr_portnum)

            # south links
            if y != 0:
                rtr.addLink(getLink("rtr_%d_%d"%(x,y-1), "rtr_%d_%d"%(x,y)), rtr_portname, "800ps")
                #print("Add south link with portname=%s to x,y=%d_%d"%(rtr_portname,x,y))
            rtr_portnum += 1
            rtr_portname = "port" + str(rtr_portnum)

            # west links
            if x != 0:
                rtr.addLink(getLink("rtr_%d_%d"%(x-1,y), "rtr_%d_%d"%(x,y)), rtr_portname, "800ps")
                #print("Add west link with portname=%s to x,y=%d_%d"%(rtr_portname,x,y))
            rtr_portnum += 1

            # local ports
            for k in range(local_ports):
                lcl_portname = "port" + str(k+rtr_portnum)
                # create endpoint
                ep_name = "local_ep_%d_%d_%d"%(x,y,k)
                ep_num = (x*y_size*local_ports) + (y*local_ports) + k
                num_eps = x_size * y_size * local_ports
                print("%s Created endpoint %d with num_eps %d"%(ep_name, ep_num, num_eps))
                if merlin_trafficgen == 0:
                    lcl_ep = sst.Component(ep_name, "merlin.offered_load")
                    lcl_ep.addParams(OfferedLoadParams)
                elif merlin_trafficgen == 1: # merlin_trafficgen == 1:
                    lcl_ep = sst.Component(ep_name, "merlin.background_traffic")
                else:
                    lcl_ep = sst.Component(ep_name, "merlin.clocked_offered_load")
                    lcl_ep.addParams(OfferedLoadParams)
                    lcl_ep.addParams(ClockedOfferedLoadParams)
                lcl_ep.addParam( "num_peers" , (num_eps) )
                lcl_ep.addParams(MatchingTrafficGenParams)
                lcl_ep_iface = lcl_ep.setSubComponent("networkIF", "mordred.mordredNIC")

                lcl_ep_iface.addParam("input_buf_size", "1kiB")
                lcl_ep_iface.addParam("output_buf_size", "1kiB")

                rtr.addLink(getLink("rtr_%d_%d"%(x, y), ep_name), lcl_portname, "800ps")
                lcl_ep_iface.addLink(getLink("rtr_%d_%d"%(x, y), ep_name), "port", "800ps")

                pattern_gen = lcl_ep.setSubComponent("pattern_gen", "merlin.targetgen.uniform")
                # Setting the following params seems to have no effect
                pattern_gen.addParams({
                    "min" : "0",
                    "max" : (num_eps-1),
                })

# Now, let's do another topology...
def createSimpleTorus(x_size, y_size, local_ports):
    # Create the routers
    for y in range(y_size):
        for x in range(x_size):
            rtr_name = "rtr_%d_%d"%(x, y)
            rtr = sst.Component(rtr_name, "mordred.simple_rtr")
            # north links
            portnum = getNextTopoPort(rtr_name)
            rtr_portname = "rtr_port" + str(portnum)
            if y != y_size - 1:
                rtr.addLink(getLink("rtr_%d_%d"%(x,y), "rtr_%d_%d"%(x,y+1)), rtr_portname, "800ps")
            else: # connect to y = 0 routers; y=y_size-1
                rtr.addLink(getLink("rtr_%d_%d"%(x,y), "rtr_%d_%d"%(x,0)), rtr_portname, "800ps")

            # south links
            portnum = getNextTopoPort(rtr_name)
            rtr_portname = "rtr_port" + str(portnum)
            if y != 0:
                rtr.addLink(getLink("rtr_%d_%d"%(x,y-1), "rtr_%d_%d"%(x,y)), rtr_portname, "800ps")
            else: # y=0 case, already have a link from the "north" links
                rtr.addLink(getLink("rtr_%d_%d"%(x,y_size-1), "rtr_%d_%d"%(x,0)), rtr_portname, "800ps")

            # east links
            portnum = getNextTopoPort(rtr_name)
            rtr_portname = "rtr_port" + str(portnum)
            if x != x_size - 1:
                rtr.addLink(getLink("rtr_%d_%d"%(x,y), "rtr_%d_%d"%(x+1,y)), rtr_portname, "800ps")
            else: # x=x_size-1 case; connect to x=0 rtrs
                rtr.addLink(getLink("rtr_%d_%d"%(x,y), "rtr_%d_%d"%(0,y)), rtr_portname, "800ps")

            # west links
            portnum = getNextTopoPort(rtr_name)
            rtr_portname = "rtr_port" + str(portnum)
            if x != 0:
                rtr.addLink(getLink("rtr_%d_%d"%(x-1,y), "rtr_%d_%d"%(x,y)), rtr_portname, "800ps")
            else: # x=0 case; already have a link from the "east" links
                rtr.addLink(getLink("rtr_%d_%d"%(x_size-1,y), "rtr_%d_%d"%(0,y)), rtr_portname, "800ps")


            # TODO: Create local endpoints and link them to the router

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
            # TODO: Add topo params as needed
            rtr_topo.addParams({
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
                ep = sst.Component(ep_name, "merlin.offered_load")
                ep.addParams({
                    "num_peers" : self.num_endpoints,
                    "link_bw" : "500MB/s",
                    "linkcontrol" : "mordred.mordredNIC",
                    "buffer_size" : "1kiB",
                    "packet_size" : "16B",
                    "pattern" : "merlin.targetgen.uniform",
                    "offered_load" : "0.5",
                    "warmup_time" : "1us",
                    "collect_time" : "200us",
                    "drain_time" : "50us"
                })
                endpoints.append(ep)
                #print("Created endpoint %d"%(ep_num))
                ep_iface = ep.setSubComponent( "networkIF", "mordred.mordredNIC" )
                ep_iface.addParams({
                    "input_buf_size" : "1kB",
                    "output_buf_size" : "1kB",
                })
                rtr_id = "rtr_%d"%i
                ep_id = "ep_%d"%j
                link_name, new_link = self.createEpRtrLink(rtr_id, ep_id)
                if not new_link:
                    print("WARN Failed to create ep link")
                self.routers[i].addLink(link_name, portname, "800ps")
                ep_iface.addLink(link_name, "port", "800ps")

                pattern_gen = ep.setSubComponent("pattern_gen", "merlin.targetgen.uniform")

    def createEpRtrLink(self, rtr_id, ep_id):
        name = "link.%s_%s"%(rtr_id, ep_id)
        if name not in self.flatfly_links:
            self.flatfly_links[name] = sst.Link(name)
            return self.flatfly_links[name], True
        return self.flatfly_links[name], False

class Crossbar:
    def __init__(self, num_routers, local_ports, concentration):
        self.num_routers = num_routers
        self.local_ports = local_ports
        self.concentration = concentration
        self.xbar_links = dict()
        self.routers = self.gen_routers()
        self.create_xbar_links()
        self.create_local_ports()

    def gen_routers(self):
        routers = []
        for i in range(self.num_routers):
            rtr_name = "rtr_%d"%(i)
            routers.append(sst.Component(rtr_name, "mordred.simple_rtr"))
            print("Created router {}".format(rtr_name))
        return routers

    def create_xbar_links(self):
        for i in range(self.num_routers):
            for j in range(i+1, self.num_routers):
                #if (i == j):
                #    continue
                link_name, new_link = self.getXbarLink("rtr_%d"%(i), "rtr_%d"%(j))
                if (new_link):
                    rtr_pname = "rtr_port" + str(getNextTopoPort("rtr_%d"%i))
                    self.routers[i].addLink(link_name, rtr_pname, "800ps")
                    rtr_pname = "rtr_port" + str(getNextTopoPort("rtr_%d"%j))
                    self.routers[j].addLink(link_name, rtr_pname, "800ps")
    
    def getXbarLink(self, name1, name2):
        name = "link.%s_%s"%(name1, name2)
        if name not in self.xbar_links:
            self.xbar_links[name] = sst.Link(name)
            return self.xbar_links[name], True
        return self.xbar_links[name], False

    def create_local_ports(self):
        # local ports
        for i in range(self.num_routers):
            for j in range(self.local_ports):
                lcl_portname = "local_port" + str(j)
                # create endpoint
                ep_name = "local_ep_%d_%d"%(i,j)
                lcl_ep = sst.Component(ep_name, "mordred.test_ep")
                self.routers[i].addLink(getLink("rtr_%d"%(i), ep_name), lcl_portname, "800ps")
                lcl_ep.addLink(getLink("rtr_%d"%(i), ep_name), "port", "800ps")


# General params
local_ports = 1 # == concentration

# Mesh/torus Configuration options
x_size = 3
y_size = 3

#Xbar config
xbar_size = 6

print("Do mesh")
createMesh(x_size, y_size, local_ports)

## TODO: ONLY THE FLATTENED BUTTERFLY HAS BEEN FIXED FOR THE NEW NAMING
## IN THE ROUTER COMPONENT (nor do we have matching subcomponents)

#print("Do simple torus")
#createSimpleTorus(x_size, y_size, local_ports)

#print("Do crossbar")
#xbar_net = Crossbar(xbar_size, local_ports)

# Flattened Butterfly Paper
# Flattened Butterfly : A Cost-Efficient Topology for
# High-Radix Networks, ISCA 2007

#print("Fig 1D in flat fly paper")
#flatfly = FlattenedButterfly(2, 4) # fig 1d in paper; 16 endpoints

# For some reason, this version really likes to have double links in its
# initial construction; haven't quite figured out why - could be related to
# the dimensions count (see FlattenedButterfly.create_all_links)
#print("Fig 1B in flat fly paper")
#flatfly2 = FlattenedButterfly(4, 2) # fig 1b in paper; 16 endpoints

#print("Fig 3 in Micro2007 FlatFly Paper")
#flatfly3 = FlattenedButterfly(4, 3) # 64 endpoints

# Stats collection - apparently I don't know the secret handshake because I can get the dummy
# counter in SimpleRtr to count things, but the stat in RtrPortControl is just a NullStatistic
# Fun. Annoying.  SST documentation is clearly insufficient.
sst.setStatisticLoadLevel(7)
#stat_params = ( { "rate" : "0ns" } )
sst.enableAllStatisticsForAllComponents(stat_params)
#sst.enableAllStatisticsForComponentType("mordred.simple_rtr.rtrPortControl", stat_params, True )
#sst.setStatisticOutput("sst.statOutputCSV", { "filepath" : "./stats.csv", "separator" : ", " } )

#EOF
