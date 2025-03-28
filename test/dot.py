# Automatically generated SST Python input
import sst

links = dict()
def getLink(name1, name2):
    name = "link.%s_%s"%(name1, name2)
    if name not in links:
        links[name] = sst.Link(name)
    return links[name]

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


# Configuration options
x_size = 4
y_size = 4
num_endpoints = 1 # unused in mesh

#createMesh(x_size, y_size, num_endpoints)
createSimpleTorus(x_size, y_size, num_endpoints)

#EOF
