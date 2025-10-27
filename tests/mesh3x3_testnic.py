# Automatically generated SST Python input

import sst

# Use to set the stats output filename
testname = "mesh3x3_testnic"

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
                "verbose" : 0,
                "xDim" : x_size,
                "yDim" : y_size
            })
            # north links
            if y != y_size - 1:
                rtr.addLink(getLink("rtr_%d_%d"%(x,y), "rtr_%d_%d"%(x,y+1)), "port0", "800ps")

            # east links
            if x != x_size - 1:
                rtr.addLink(getLink("rtr_%d_%d"%(x,y), "rtr_%d_%d"%(x+1,y)), "port1", "800ps")

            # south links
            if y != 0:
                rtr.addLink(getLink("rtr_%d_%d"%(x,y-1), "rtr_%d_%d"%(x,y)), "port2", "800ps")

            # west links
            if x != 0:
                rtr.addLink(getLink("rtr_%d_%d"%(x-1,y), "rtr_%d_%d"%(x,y)), "port3", "800ps")

            # local ports
            for k in range(local_ports):
                lcl_portname = "port" + str(k+4)
                # create endpoint
                ep_name = "testnic_ep_%d_%d_%d"%(x,y,k)
                ep_num = (x*y_size*local_ports) + (y*local_ports) + k
                num_eps = x_size * y_size * local_ports
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
                rtr.addLink(getLink("rtr_%d_%d"%(x, y), ep_name), lcl_portname, "800ps")
                ep_iface.addLink(getLink("rtr_%d_%d"%(x, y), ep_name), "port", "800ps")

# General params
local_ports = 1 # == concentration

# Mesh Configuration options
x_size = 3
y_size = 3

createMesh(x_size, y_size, local_ports)

# Do stats
sst.setStatisticLoadLevel(7)
stat_params = ( { "rate" : "0ns" } )
sst.setStatisticOutput("sst.statOutputCSV", { "filepath" : "./stats.%s.csv"%testname, "separator" : ", " } )
sst.enableAllStatisticsForAllComponents(stat_params)

#EOF
