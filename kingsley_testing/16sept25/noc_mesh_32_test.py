# Automatically generated SST Python input
import sst

sst.setProgramOption("timebase", "1ps")
sst.setProgramOption("stop-at", "1ms")

load_level = 10
load_factor = (load_level/100)

#stat_params = ( { "rate" : "0ns" } )
sst.setStatisticOutput("sst.statOutputCSV", { "filepath" : "./kingsley.OL.LF%s.csv"%load_level, "separator" : ", " } )

x_size = 3
y_size = 3

# put in the routers

links = dict()
def getLink(name1, name2):
    name = "link.%s_%s"%(name1, name2)
    if name not in links:
        links[name] = sst.Link(name)
    return links[name]

num_endpoints = 1

#num_peers = (num_endpoints * (x_size * y_size)) + (2*x_size) + (2*y_size)
num_peers = x_size * y_size
num_messages = 10
msg_size = "64b"
link_bw = "1GB/s"
flit_size = "16b"
input_buf_size = "32B"
#input_buf_size = "256B"

TestNicParams = {
    "num_peers" : "%d"%(num_peers),
    "link_bw" : "1GB/s",
    "linkcontrol_type" : "kingsley.linkcontrol",
    "message_size" : msg_size,
    "num_messages" : 0
}

OfferedLoadParams = {
    "num_peers" : num_peers,
    "packet_size" : "64b",
    "offered_load" : load_factor,
    "pattern" : "merlin.targetgen.uniform",
    "link_bw" : "1GB/s",
    "linkcontrol" : "kingsley.linkcontrol",
    "buffer_size" : "1kiB",
    "warmup_time" : "1us",
    "collect_time" : "500us",
    "drain_time" : "50us"
}

BackgroundTrafficParams = {
    "num_peers" : "%d"%(num_peers),
    "packet_size" : "64b",
    "offered_load" : load_factor,
    "pattern" : "merlin.targetgen.uniform"
}

# Setting this to True will cause no-cut links on the north and south
# ports, as well as on all endpoints
add_no_cut = False

for y in range(y_size):
    for x in range(x_size):
        rtr = sst.Component("rtr_%d_%d"%(x,y), "kingsley.noc_mesh")
        rtr.addParams({
            "local_ports" : "%d"%(num_endpoints),
            "link_bw" : link_bw,
            "input_buf_size" : input_buf_size,
            "flit_size" : flit_size,
            "use_dense_map" : "true"
            #"port_priority_equal" : "true"
        })
        # wire up mesh connections.  Any index that would be -1 will
        # show up as X in the name
        if y != y_size - 1:
            rtr.addLink(getLink("rtr_%d_%d"%(x,y), "rtr_%d_%d"%(x,y+1)), "north", "800ps")
            if add_no_cut:
                getLink("rtr_%d_%d"%(x,y), "rtr_%d_%d"%(x,y+1)).setNoCut()
        #else:
        #    rtr.addLink(getLink("rtr_%d_%d"%(x,y), "ep0_%d_%d"%(x,y+1)), "north", "800ps")
        #    if add_no_cut:
        #        getLink("rtr_%d_%d"%(x,y), "ep0_%d_%d"%(x,y+1)).setNoCut()
        #    ep = sst.Component("ep0_%d_%d"%(x,y+1), "merlin.test_nic")
        #    ep.addParams(TestNicParams)
        #    sub = ep.setSubComponent("networkIF","kingsley.linkcontrol")
        #    sub.addParam("link_bw","1GB/s")
        #    sub.addLink(getLink("rtr_%d_%d"%(x,y), "ep0_%d_%d"%(x,y+1)), "rtr_port", "800ps")
            
            
        if y != 0:
            rtr.addLink(getLink("rtr_%d_%d"%(x,y-1), "rtr_%d_%d"%(x,y)), "south", "800ps")
        #else:
            # Y = 0
        #    rtr.addLink(getLink("rtr_%d_X"%(x), "ep0_%d_%d"%(x,y)), "south", "800ps")
        #    if add_no_cut:
        #        getLink("rtr_%d_X"%(x), "ep0_%d_%d"%(x,y)).setNoCut()
        #    ep = sst.Component("ep0_%d_X"%(x), "merlin.test_nic")
        #    ep.addParams(TestNicParams)
        #    sub = ep.setSubComponent("networkIF","kingsley.linkcontrol")
        #    sub.addParam("link_bw","1GB/s")
        #    sub.addLink(getLink("rtr_%d_X"%(x), "ep0_%d_%d"%(x,y)), "rtr_port", "800ps")

        if x != x_size - 1:
            rtr.addLink(getLink("rtr_%d_%d"%(x,y), "rtr_%d_%d"%(x+1,y)), "east", "800ps")
        #else:
        #    rtr.addLink(getLink("rtr_%d_%d"%(x,y), "ep0_%d_%d"%(x+1,y)), "east", "800ps")
        #    if add_no_cut:
        #        getLink("rtr_%d_%d"%(x,y), "ep0_%d_%d"%(x+1,y)).setNoCut()
        #    ep = sst.Component("ep0_%d_%d"%(x+1,y), "merlin.test_nic")
        #    ep.addParams(TestNicParams)
        #    sub = ep.setSubComponent("networkIF","kingsley.linkcontrol")
        #    sub.addParam("link_bw","1GB/s")
        #    sub.addLink(getLink("rtr_%d_%d"%(x,y), "ep0_%d_%d"%(x+1,y)), "rtr_port", "800ps")

        if x != 0:
            rtr.addLink(getLink("rtr_%d_%d"%(x-1,y), "rtr_%d_%d"%(x,y)), "west", "800ps")
        #else:
            # X = 0
         #   rtr.addLink(getLink("rtr_X_%d"%(y), "ep0_%d_%d"%(x,y)), "west", "800ps")
         #   if add_no_cut:
         #       getLink("rtr_X_%d"%(y), "ep0_%d_%d"%(x,y)).setNoCut()
         #   ep = sst.Component("ep0_X_%d"%(y), "merlin.test_nic")
         #   ep.addParams(TestNicParams)
         #   sub = ep.setSubComponent("networkIF","kingsley.linkcontrol")
         #   sub.addParam("link_bw","1GB/s")
         #   sub.addLink(getLink("rtr_X_%d"%(y), "ep0_%d_%d"%(x,y)), "rtr_port", "800ps")

        # Add endpoints
        for z in range(num_endpoints):
            rtr.addLink(getLink("rtr_%d_%d"%(x,y), "ep%d_%d_%d"%(z,x,y)), "local%d"%(z), "800ps")
            if add_no_cut:
                getLink("rtr_%d_%d"%(x,y), "ep%d_%d_%d"%(z,x,y)).setNoCut()
            ep = sst.Component("ep%d_%d_%d"%(z,x,y), "merlin.offered_load")
            ep.addParams(OfferedLoadParams)
            sub = ep.setSubComponent("networkIF","kingsley.linkcontrol")
            sub.addParam("link_bw","1GB/s")
            sub.addLink(getLink("rtr_%d_%d"%(x,y), "ep%d_%d_%d"%(z,x,y)), "rtr_port", "800ps")
            pattern_gen = ep.setSubComponent("pattern_gen", "merlin.targetgen.uniform")


sst.setStatisticLoadLevel(9)

#sst.setStatisticOutput("sst.statOutputCSV");
#sst.setStatisticOutputOptions({
#    "filepath" : "stats.csv",
#    "separator" : ", "
#})

sst.enableAllStatisticsForAllComponents({"type":"sst.AccumulatorStatistic","rate":"0ns"})
