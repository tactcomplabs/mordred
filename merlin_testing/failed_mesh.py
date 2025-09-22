# Automatically generated SST Python input
import sst

sst.setProgramOption("timebase", "1ps")
sst.setProgramOption("stop-at", "200us")

load_level = 10
load_factor = (load_level/100)

#stat_params = ( { "rate" : "0ns" } )
sst.setStatisticOutput("sst.statOutputCSV", { "filepath" : "./stats.LF%s.csv"%load_level, "separator" : ", " } )

x_size = 3
y_size = 3

# put in the routers

links = dict()
def getLink(name1, name2):
    name = "link.%s_%s"%(name1, name2)
    if name not in links:
        print("Creating link %s..."%name)
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
    "collect_time" : "100us",
    "drain_time" : "50us"
}

BackgroundTrafficParams = {
    "num_peers" : "%d"%(num_peers),
    "packet_size" : "64b",
    "offered_load" : load_factor,
    "pattern" : "merlin.targetgen.uniform"
}

MeshTopoParams = {
    "network_name" : "noc_mesh",
    "mesh.shape" : "3x3",
    "mesh.width" : "1x1",
    "mesh.local_ports" : "1",
    "shape" : "3x3",
    "width" : "1x1",
    "local_ports" : "1"
}

HrRouterParams = {
    "id" : 0,
    "num_ports" : "9",
    "topology" : "merlin.mesh",
    "link_bw" : "1GB/s",
    "flit_size" : "16b",
    "xbar_bw" : "2GiB/s",
    "input_latency" : "100ps",
    "output_latency" : "100ps",
    "input_buf_size" : "32B",
    "output_buf_size" : "2B",
    "num_vns" : "1",
    "debug" : "1"
}

LinkControlParams = {
    "port_name" : "rtr_port",
    "link_bw" : "1GB/s",
    "input_buf_size" : "1KiB",
    "output_buf_size" : "1KiB",
}

router = sst.Component("router", "merlin.hr_router")
router.addParams(HrRouterParams)
router_topo = router.setSubComponent("topology", "merlin.mesh")
router_topo.addParams(MeshTopoParams)
# TODO? Add arb and portcontrol?

# Setting this to True will cause no-cut links on the north and south
# ports, as well as on all endpoints
add_no_cut = False

for y in range(y_size):
    for x in range(x_size):
        #router.addLink(getLink("rtr_%d_%d"%(x,y), "ep%d_%d_%d"%(z,x,y)), "local%d"%(z), "800ps")
        #if add_no_cut:
        #    getLink("rtr_%d_%d"%(x,y), "ep%d_%d_%d"%(z,x,y)).setNoCut()
        ep = sst.Component("ep%d_%d"%(x,y), "merlin.offered_load")
        ep.addParams(OfferedLoadParams)
        sub = ep.setSubComponent("networkIF","merlin.linkcontrol")
        sub.addParams(LinkControlParams)
        portnum = (x*y_size) + y
        sub.addLink(getLink("rtr_%d_%d"%(x,y), "ep%d_%d"%(x,y)), "rtr_port", "800ps")
        router.addLink(getLink("rtr_%d_%d"%(x,y), "ep%d_%d"%(x,y)), "port%d"%(portnum), "800ps")
        pattern_gen = ep.setSubComponent("pattern_gen", "merlin.targetgen.uniform")

sst.setStatisticLoadLevel(9)

#sst.enableAllStatisticsForComponentType("kingsley.noc_mesh", {"type":"sst.AccumulatorStatistic","rate":"0ns"})
sst.enableAllStatisticsForAllComponents()