# Automatically generated SST Python input

import sst
from sst import UnitAlgebra

# Use to set the stats output filename
testname = "torus3D_3x3x3_2vc_testnic"

# Set up some parameters via UnitAlgebra
clk = UnitAlgebra("1GHz")
clk_pd = clk.invert()
link_latency = UnitAlgebra(0.8) * clk_pd
flit_size = UnitAlgebra("16b")
num_vns = "1"
noc_link_bw = clk * flit_size

FixedRtrParams = {
    "verbose" : "0",
    "clock" : clk,
    "num_vcs" : "2",
    "num_vns" : num_vns,
    "flit_size" : flit_size,
    "input_buf_size" : UnitAlgebra(16)*flit_size,
    "output_buf_size" : UnitAlgebra(1)*flit_size
}

FixedTestNicParams = {
    "num_messages" : 10,
    "message_size" : UnitAlgebra(4)*flit_size,
    "send_untimed_broadcast" : "false"
}

MordredNICParams = {
    "verbose" : "0",
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

# There are local_ports endpoints connected to each router
# rtr_id is expected to go linearly from 0-(x_size*y_size_z_size - 1)
# rtr_id is computed as (z * x_size*y_size) + (y * x_size) + x
# links are expected to be ordered as n,e,s,w
def createTorus(x_size, y_size, z_size, local_ports):
    # Create the routers
    rtr_id = 0
    nports = 6 + local_ports
    rtr_params = {
        "num_ports": nports,
        "num_local_ports" : local_ports,
    }
    num_eps = x_size * y_size * z_size * local_ports
    for z in range(z_size):
        for y in range(y_size):
            for x in range(x_size):
                rtr = sst.Component("rtr_%d_%d_%d"%(x, y, z), "mordred.simple_rtr")
                rtr.addParam("id", rtr_id)
                rtr.addParams(FixedRtrParams)
                rtr.addParams(rtr_params)
                rtr_id += 1
                rtr_topo = rtr.setSubComponent( "topology", "mordred.torus3DTopo" )
                rtr_topo.addParams({
                    "verbose" : 0,
                    "xDim" : x_size,
                    "yDim" : y_size,
                    "zDim" : z_size
                })
                # north links
                if y != y_size - 1:
                    rtr.addLink(getLink("rtr_%d_%d_%d"%(x,y,z), "rtr_%d_%d_%d"%(x,y+1,z)), "port0", link_latency)
                else: # connect to y = 0 routers; y=y_size-1
                    rtr.addLink(getLink("rtr_%d_%d_%d"%(x,y,z), "rtr_%d_%d_%d"%(x,0,z)), "port0", link_latency)
    
                # east links
                if x != x_size - 1:
                    rtr.addLink(getLink("rtr_%d_%d_%d"%(x,y,z), "rtr_%d_%d_%d"%(x+1,y,z)), "port1", link_latency)
                else: # x=x_size-1 case; connect to x=0 rtrs
                    rtr.addLink(getLink("rtr_%d_%d_%d"%(x,y,z), "rtr_%d_%d_%d"%(0,y,z)), "port1", link_latency)
    
                # south links
                if y != 0:
                    rtr.addLink(getLink("rtr_%d_%d_%d"%(x,y-1,z), "rtr_%d_%d_%d"%(x,y,z)), "port2", link_latency)
                else: # y=0 case, should already have a link from the "north" links
                    rtr.addLink(getLink("rtr_%d_%d_%d"%(x,y_size-1,z), "rtr_%d_%d_%d"%(x,0,z)), "port2", link_latency)
    
                # west links
                if x != 0:
                    rtr.addLink(getLink("rtr_%d_%d_%d"%(x-1,y,z), "rtr_%d_%d_%d"%(x,y,z)), "port3", link_latency)
                else: # x=0 case; should already have a link from the "east" links
                    rtr.addLink(getLink("rtr_%d_%d_%d"%(x_size-1,y,z), "rtr_%d_%d_%d"%(0,y,z)), "port3", link_latency)

                # plusZ links
                if z != z_size - 1:
                    rtr.addLink(getLink("rtr_%d_%d_%d"%(x,y,z), "rtr_%d_%d_%d"%(x,y,z+1)), "port4", link_latency)
                else: # z=z_size-1 case; connect to z=0 rtrs
                    rtr.addLink(getLink("rtr_%d_%d_%d"%(x,y,z), "rtr_%d_%d_%d"%(x,y,0)), "port4", link_latency)

                # minusZ links
                if z != 0:
                    rtr.addLink(getLink("rtr_%d_%d_%d"%(x,y,z-1), "rtr_%d_%d_%d"%(x,y,z)), "port5", link_latency)
                else: # z=0 case; should already have a link from the "plusZ" links
                    rtr.addLink(getLink("rtr_%d_%d_%d"%(x,y,z_size-1), "rtr_%d_%d_%d"%(x,y,0)), "port5", link_latency)

                # local ports
                for k in range(local_ports):
                    lcl_portname = "port" + str(k+6)
                    # create endpoint
                    ep_name = "testnic_ep_%d_%d_%d_%d"%(x,y,z,k)
                    #ep_num = (x*y_size*local_ports) + (y*local_ports) + k
                    ep_num = ((rtr_id - 1) * local_ports) + k
                    #print("%s Created endpoint %d with num_eps %d"%(ep_name, ep_num, num_eps))
                    ep = sst.Component(ep_name, "merlin.test_nic")
                    ep.addParams(FixedTestNicParams)
                    ep.addParams({
                        "id" : ep_num,
                        "num_peers" : num_eps,
                    })
                    # Add endpoint interface to the NoC
                    ep_iface = ep.setSubComponent("networkIF", "mordred.mordredNIC")
                    ep_iface.addParams(MordredNICParams)
    
                    # Add link
                    rtr.addLink(getLink("rtr_%d_%d_%d"%(x, y, z), ep_name), lcl_portname, link_latency)
                    ep_iface.addLink(getLink("rtr_%d_%d_%d"%(x, y, z), ep_name), "port", link_latency)

# General params
local_ports = 1 # == concentration

# Configuration options
x_size = 3
y_size = 3
z_size = 3

createTorus(x_size, y_size, z_size, local_ports)

# Do stats
sst.setStatisticLoadLevel(7)
stat_params = ( { "rate" : "0ns" } )
sst.setStatisticOutput("sst.statOutputCSV", { "filepath" : "./stats.%s.csv"%testname, "separator" : ", " } )
sst.enableAllStatisticsForAllComponents(stat_params)

#EOF
