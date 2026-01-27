# This test is derived from the following:
# https://github.com/sstsimulator/sst-tutorials/blob/master/ipdps2025/exercises/intro/solutions/demo_7.py

# This test tries out two main features:
# - Swap out the shogun crossbar for a 2x2 mesh
# - Use the mordred.mordredNIC subcomponent in the "linkcontrol" slot of  the memHierarchy.MemNIC subcomponent

import sst
from sst import UnitAlgebra

numCores = 2
numLLC = 2

memory_mb = 1024
memory_capacity_inB = memory_mb * 1024 * 1024

memory_per_block = memory_mb / numLLC
memory_capacity_block_inB = memory_per_block * 1024 * 1024

#########################################################################
## Define SST core options
#########################################################################
# If this demo gets to 100ms, something has gone very wrong!
sst.setProgramOption("stop-at", "100ms")

#########################################################################
## Parameter Definitions
#########################################################################
# Cache: L1, 2.4GHz, 2KB, 4-way set associative, 64B lines, LRU replacement, MESI coherence
l1_params = dict({
    "L1" : 1,
    "cache_frequency" : "2.4GHz",
    "access_latency_cycles" : 2,
    "cache_size" : "2KiB",
    "associativity" : 4,
    "replacement_policy" : "lru",
    "coherence_policy" : "MESI",
    "cache_line_size" : 64,
})

l2_params = dict({
    "L1" : 0,
    "cache_frequency" : "2.4GHz",
    "access_latency_cycles" : 10,
    "cache_size" : "64KiB",
    "associativity" : 4,
    "replacement_policy" : "lru",
    "coherence_policy" : "MESI",
    "cache_line_size" : 64,
    "mshr_latency_cycles" : 4,
    "mshr_num_entries"  : 256
})

# Core: 2.4GHz, 2 accesses/cycle, STREAM (triad) pattern generator with 1000 elements per array
core_params = dict({
    "clock" : "2.4GHz",
    "max_reqs_cycle" : 2,
})

gen_params = dict({
    "n" : 1000,             # Number of array elements
})

#########################################################################
## Getting too many components to write out everything, so let's
## simplify the description of the model...
#########################################################################

### Original used shogun crossbar with 4 endpoints; we will create a 2x2 mesh
### and hang a router off of each one; set basic parameters here
link_latency = UnitAlgebra("800ps")
flit_size = UnitAlgebra("16b")
FixedRtrParams = {
    "verbose" : "0",
    "clock" : "1.0GHz",
    "num_vcs" : "1",
    "flit_size" : flit_size,
    "input_buf_size" : UnitAlgebra(16)*flit_size,
    "output_buf_size" : UnitAlgebra(1)*flit_size
}

# NoC link generation
links = dict()
def getLink(name1, name2):
    name = "link.%s_%s"%(name1, name2)
    if name not in links:
        links[name] = sst.Link(name)
        #print("New link: %s"%name)
    return links[name]

# Creating the NoC - parameters
numRtrs = 4
local_ports = 1
y_size = 2
x_size = 2
nports = 4 + local_ports
rtr_params = {
    "num_ports": nports,
    "num_local_ports" : local_ports,
}

# Creating the NoC - initialize
rtr_id = 0
routers = dict()

# Creating the NoC - routers
for y in range(0, y_size):
    for x in range(0, x_size):
        rtr_name = "rtr_%d_%d"%(x, y)
        print("Creating router with name %s"%(rtr_name))
        if rtr_name not in routers:
            routers[rtr_name] = sst.Component(rtr_name, "mordred.simple_rtr")
        routers[rtr_name].addParam("id", rtr_id)
        routers[rtr_name].addParams(FixedRtrParams)
        routers[rtr_name].addParams(rtr_params)
        rtr_id += 1
        rtr_topo = routers[rtr_name].setSubComponent( "topology", "mordred.MeshTopology" )
        rtr_topo.addParams({
            "verbose" : 0,
            "xDim" : x_size,
            "yDim" : y_size
        })
        # north links
        if y != y_size - 1:
            routers[rtr_name].addLink(getLink("rtr_%d_%d"%(x,y), "rtr_%d_%d"%(x,y+1)), "port0", link_latency)

        # east links
        if x != x_size - 1:
            routers[rtr_name].addLink(getLink("rtr_%d_%d"%(x,y), "rtr_%d_%d"%(x+1,y)), "port1", link_latency)

        # south links
        if y != 0:
            routers[rtr_name].addLink(getLink("rtr_%d_%d"%(x,y-1), "rtr_%d_%d"%(x,y)), "port2", link_latency)

        # west links
        if x != 0:
            routers[rtr_name].addLink(getLink("rtr_%d_%d"%(x-1,y), "rtr_%d_%d"%(x,y)), "port3", link_latency)

### NOTE: NO ENDPOINTS CREATED YET
# Set up parameters for the endpoint NICs
MordredNICParams = {
    "verbose" : "0",
    "input_buf_size" : "1kiB",
    "output_buf_size" : "1kiB",
}

p_n = 0

for core_id in range (0, numCores):
   core = sst.Component("core_" + str(core_id), "miranda.BaseCPU")
   core.addParams(core_params)

   gen = core.setSubComponent("generator", "miranda.STREAMBenchGenerator")
   gen.addParams(gen_params)

   iface = core.setSubComponent("memory", "memHierarchy.standardInterface")

   l1_cache = sst.Component("l1_cache_" + str(core_id), "memHierarchy.Cache")
   l1_cache.addParams(l1_params)
   l1_up = l1_cache.setSubComponent("highlink", "memHierarchy.MemLink")
   l1_down = l1_cache.setSubComponent("lowlink", "memHierarchy.MemLink")

   l2_cache = sst.Component("l2_cache_" + str(core_id), "memHierarchy.Cache")
   l2_cache.addParams(l2_params)

   l2_up = l2_cache.setSubComponent("highlink", "memHierarchy.MemLink")
   l2_down = l2_cache.setSubComponent("lowlink", "memHierarchy.MemNIC")
   l2_down.addParams({ "group" : 2 })
   # Replace shogun.ShogunNIC with mordred.mordredNIC
   l2_linkctrl = l2_down.setSubComponent("linkcontrol", "mordred.mordredNIC")
   l2_linkctrl.addParams(MordredNICParams)

   ## Define and connect links
   core_cache_link = sst.Link("core_to_cache_" + str(core_id))
   core_cache_link.connect( (iface, "lowlink", "100ps"), (l1_up, "port", "100ps") )

   l1_l2_link = sst.Link("l1_to_l2_" + str(core_id))
   l1_l2_link.connect( (l1_down, "port", "100ps"), (l2_up, "port", "100ps") )

   bus_l2 = sst.Link("bus_to_l2_" + str(core_id))
   rtr_name = "rtr_%d_%d"%(core_id, 0)
   bus_l2.connect( (l2_linkctrl, "port", "50ps"), (routers[rtr_name], "port4", "50ps") )

   p_n = p_n + 1

for cache_id in range (0, numLLC):
   startAddr = 0 + (256 * cache_id)
   endAddr = startAddr + memory_capacity_inB - (256 * numLLC)

   dirctrl = sst.Component("dirctrl_" + str(cache_id), "memHierarchy.DirectoryController")
   dirctrl.addParams({
         "coherence_protocol" : "MESI",
         "entry_cache_size" : "32768",
         "addr_range_end" : endAddr,
         "addr_range_start" : startAddr,
         "interleave_size" : "256B",
         "interleave_step" : str(numLLC * 256) + "B",
   })
   dc_highlink = dirctrl.setSubComponent("highlink", "memHierarchy.MemNIC")
   dc_memlink = dirctrl.setSubComponent("lowlink", "memHierarchy.MemLink")
   dc_highlink.addParams({
      "group" : 3,
   })
   # Replace shogun.ShogunNIC with mordred.mordredNIC
   dc_linkctrl = dc_highlink.setSubComponent("linkcontrol", "mordred.mordredNIC")
   dc_linkctrl.addParams(MordredNICParams)

   memctrl = sst.Component("memory_" + str(cache_id), "memHierarchy.MemController")
   memctrl.addParams({
      "clock" : "1.0GHz",
      "backing" : "none", # We're not using real memory values, just addresses
   })
   memLink = memctrl.setSubComponent("highlink", "memHierarchy.MemLink")

   memory = memctrl.setSubComponent("backend", "memHierarchy.timingDRAM")
   memory.addParams({
      "id" : 0,
      "mem_size" : str(memory_capacity_inB) + "B",
      "clock" : "1.0GHz",
   })

   bus_mem = sst.Link("cache_to_memory_" + str(cache_id))
   rtr_name = "rtr_%d_%d"%(cache_id, 1)
   bus_mem.connect( (routers[rtr_name], "port4", "50ps"), (dc_linkctrl, "port", "50ps") )

   link_dir_mem = sst.Link("link_dir_mem_" + str(cache_id))
   link_dir_mem.connect( (dc_memlink, "port", "50ps"), (memLink, "port", "50ps") )

   p_n = p_n + 1

#########################################################################
## Statistics
#########################################################################

# Enable SST Statistics Outputs for this simulation
# Generate statistics in CSV format
sst.setStatisticOutput("sst.statoutputcsv")

# Send the statistics to a file
sst.setStatisticOutputOptions( { "filepath"  : "stats.ipdps25tutorial_demo7.csv" })

# Print statistics of level 5 and below (0-5)
sst.setStatisticLoadLevel(5)

# Enable statistics for all the component
sst.enableAllStatisticsForAllComponents()

print ("\nCompleted configuring the Demo_7 model\n")


################################ The End ################################

