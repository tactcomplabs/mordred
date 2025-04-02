# Automatically generated SST Python input
import sst
from math import floor


links = dict()
def getLink(name1, name2):
    name = "link.%s_%s"%(name1, name2)
    if name not in links:
        links[name] = sst.Link(name)
        print("New link: %s"%name)
    return links[name]

# Using a new function rather than rewriting all the code using getLink;
# this version ensures we're not doing duplicate links
def getFlatFlyLink(name1, name2):
    name = "link.%s_%s"%(name1, name2)
    if name not in links:
        links[name] = sst.Link(name)
        return links[name], True
    return links[name], False


topo_port_nums = dict()
def getNextTopoPort(name):
    if name not in topo_port_nums:
        topo_port_nums[name] = 1
    else:
        topo_port_nums[name] += 1
    return (topo_port_nums[name] - 1)

# Using kingsley/tests/refFiles/noc_mesh_32_test.py as a guide
# This version has a simple component (mordred.test_ep) that is a placeholder
# for the unconnected "topology" ports
# There is nothing connected to the local ports of the routers
def createMesh(x_size, y_size, num_endpoints):
    # Create the routers
    for y in range(y_size):
        for x in range(x_size):
            rtr = sst.Component("rtr_%d_%d"%(x, y), "mordred.simple_rtr")
            rtr_portnum = 0
            rtr_portname = "topo_port" + str(rtr_portnum)
            # north links
            if y != y_size - 1:
                rtr.addLink(getLink("rtr_%d_%d"%(x,y), "rtr_%d_%d"%(x,y+1)), rtr_portname, "800ps")
            else: # connect to y == 0
                rtr.addLink(getLink("rtr_%d_%d"%(x,y), "epN_%d_%d"%(x,y+1)), rtr_portname, "800ps")
                epn = sst.Component("epN_%d_%d"%(x,y+1), "mordred.test_ep")
                epn.addLink(getLink("rtr_%d_%d"%(x,y), "epN_%d_%d"%(x,y+1)), "port", "800ps")
            rtr_portnum += 1
            rtr_portname = "topo_port" + str(rtr_portnum)

            # south links
            if y != 0:
                rtr.addLink(getLink("rtr_%d_%d"%(x,y-1), "rtr_%d_%d"%(x,y)), rtr_portname, "800ps")
            else: # connect to endpts
                rtr.addLink(getLink("rtr_%d_X"%(x), "epS_%d_%d"%(x,y)), rtr_portname, "800ps")
                eps = sst.Component("epS_%d_X"%(x), "mordred.test_ep")
                eps.addLink(getLink("rtr_%d_X"%(x), "epS_%d_%d"%(x,y)), "port", "800ps")
            rtr_portnum += 1
            rtr_portname = "topo_port" + str(rtr_portnum)

            # east links
            if x != x_size - 1:
                rtr.addLink(getLink("rtr_%d_%d"%(x,y), "rtr_%d_%d"%(x+1,y)), rtr_portname, "800ps")
            else:
                rtr.addLink(getLink("rtr_%d_%d"%(x,y), "epE_%d_%d"%(x+1,y)), rtr_portname, "800ps")
                epe = sst.Component("epE_%d_%d"%(x+1,y), "mordred.test_ep")
                epe.addLink(getLink("rtr_%d_%d"%(x,y), "epE_%d_%d"%(x+1,y)), "port", "800ps")
            rtr_portnum += 1
            rtr_portname = "topo_port" + str(rtr_portnum)

            # west links
            if x != 0:
                rtr.addLink(getLink("rtr_%d_%d"%(x-1,y), "rtr_%d_%d"%(x,y)), rtr_portname, "800ps")
            else:
                rtr.addLink(getLink("rtr_X_%d"%(y), "epW_%d_%d"%(x,y)), rtr_portname, "800ps")
                epw = sst.Component("epW_X_%d"%(y), "mordred.test_ep")
                epw.addLink(getLink("rtr_X_%d"%(y), "epW_%d_%d"%(x,y)), "port", "800ps")

# Now, let's do another topology...
def createSimpleTorus(x_size, y_size, num_endpoints):
    # Create the routers
    for y in range(y_size):
        for x in range(x_size):
            rtr_name = "rtr_%d_%d"%(x, y)
            rtr = sst.Component(rtr_name, "mordred.simple_rtr")
            # north links
            portnum = getNextTopoPort(rtr_name)
            rtr_portname = "topo_port" + str(portnum)
            if y != y_size - 1:
                rtr.addLink(getLink("rtr_%d_%d"%(x,y), "rtr_%d_%d"%(x,y+1)), rtr_portname, "800ps")
            else: # connect to y = 0 routers; y=y_size-1
                rtr.addLink(getLink("rtr_%d_%d"%(x,y), "rtr_%d_%d"%(x,0)), rtr_portname, "800ps")

            # south links
            portnum = getNextTopoPort(rtr_name)
            rtr_portname = "topo_port" + str(portnum)
            if y != 0:
                rtr.addLink(getLink("rtr_%d_%d"%(x,y-1), "rtr_%d_%d"%(x,y)), rtr_portname, "800ps")
            else: # y=0 case, already have a link from the "north" links
                rtr.addLink(getLink("rtr_%d_%d"%(x,y_size-1), "rtr_%d_%d"%(x,0)), rtr_portname, "800ps")

            # east links
            portnum = getNextTopoPort(rtr_name)
            rtr_portname = "topo_port" + str(portnum)
            if x != x_size - 1:
                rtr.addLink(getLink("rtr_%d_%d"%(x,y), "rtr_%d_%d"%(x+1,y)), rtr_portname, "800ps")
            else: # x=x_size-1 case; connect to x=0 rtrs
                rtr.addLink(getLink("rtr_%d_%d"%(x,y), "rtr_%d_%d"%(0,y)), rtr_portname, "800ps")

            # west links
            portnum = getNextTopoPort(rtr_name)
            rtr_portname = "topo_port" + str(portnum)
            if x != 0:
                rtr.addLink(getLink("rtr_%d_%d"%(x-1,y), "rtr_%d_%d"%(x,y)), rtr_portname, "800ps")
            else: # x=0 case; already have a link from the "east" links
                rtr.addLink(getLink("rtr_%d_%d"%(x_size-1,y), "rtr_%d_%d"%(0,y)), rtr_portname, "800ps")

class FlattenedButterfly:
    def __init__(self, k, n):
        self.k = k
        self.n = n
        self.num_endpoints = pow(k, n)
        self.num_routers = floor(self.num_endpoints/k)
        self.radix = ( n * (k-1) ) + 1
        self.num_rtr_rtr_links = k-1
        self.num_dims = n - 1
        self.routers = self.gen_routers()
        self.create_all_links()

    def gen_routers(self):
        routers = []
        for i in range(self.num_routers):
            rtr_name = "rtr_%d"%(i)
            #routers.append(rtr_name)
            routers.append(sst.Component(rtr_name, "mordred.simple_rtr"))
            print("Created router {}".format(rtr_name))
        return routers

# The range of d might be off a hair - paper has it as 1, self.n-1 but that blows up
# on a 4-ary, 2-fly.
    def create_all_links(self):
        for d in range(self.num_dims):
            for m in range (self.num_rtr_rtr_links):
                for i in range(self.num_routers):
                    ff = floor(i/pow(self.k,d)) % self.k
                    j = i + ( ( m - ff ) * pow(self.k,d) )
                    if ( j != i ):
                        self.create_link(i,j)

    def create_link(self, i, j):
        if (i > j):
            i,j = j,i
        link_name, new_link = getFlatFlyLink("rtr_%d"%(i), "rtr_%d"%(j))
        if new_link:
            rtr_pname = "topo_port" + str(getNextTopoPort("rtr_%d"%i))
            self.routers[i].addLink(link_name, rtr_pname, "800ps")
            rtr_pname = "topo_port" + str(getNextTopoPort("rtr_%d"%j))
            self.routers[j].addLink(link_name, rtr_pname, "800ps")

# Configuration options
#x_size = 4
#y_size = 4
#num_endpoints = 1 # unused in mesh

#createMesh(x_size, y_size, num_endpoints)
#createSimpleTorus(x_size, y_size, num_endpoints)

# Flattened Butterfly Paper
# Flattened Butterfly : A Cost-Efficient Topology for
# High-Radix Networks, ISCA 2007

#print("Fig 1D in flat fly paper")
#flatfly = FlattenedButterfly(2, 4) # fig 1d in paper

# For some reason, this version really likes to have double links in its
# initial construction; haven't quite figured out why - could be related to
# the dimensions count (see FlattenedButterfly.create_all_links)
#print("Fig 1B in flat fly paper")
#flatfly2 = FlattenedButterfly(4, 2) # fig 1b in paper

print("Fig 3 in Micro2007 FlatFly Paper")
flatfly3 = FlattenedButterfly(4, 3) # fig 1b in paper

#EOF
