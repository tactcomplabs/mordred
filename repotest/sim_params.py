# Automatically generated SST Python input

import sst
from sst import UnitAlgebra

# Endpoint types
EndpointTypes = {
    "TestNIC" : "merlin.test_nic",
    "OfferedLoad" : "merlin.offered_load",
    "TrafficGen" : "merlin.trafficgen"
}

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
    "num_messages" : 1000,
    "message_size" : UnitAlgebra(4)*flit_size,
    "send_untimed_broadcast" : "false", # matches default
    #"send_untimed_broadcast" : "true",
}

MordredNICParams = {
    "verbose" : "0",
    "input_buf_size" : "1kiB",
    "output_buf_size" : "1kiB",
}

TrafficGenParams = {
    # Commented parameters should be set when defining the endpoint
    # "id"
    # "num_peers"
    "num_vns" : num_vns,
    "link_bw" : noc_link_bw,
    #"topology" : "merlin.mesh", # Is this even used?
    "buffer_length" : "1kiB",
    "packets_to_send" : "1000",
    # "packet_size" : "16B",
    "delay_between_packets" : "10ns",
    "message_rate" : "1GHz"
}

# This configuration bombed hardcore
TrafficGenMessagingParams = {
    # Binomial is not supported for merlin
    "PacketDest.pattern" : "Uniform",
    # "PacketDest.RangeMax" -- set to num_peers (when defining the system; see note for HotSpot)
    "PacketDest.HotSpot.target" : "6",
    "PacketDest.HotSpot.targetProbability" : ".5",
    "PacketDest.Normal.Mean" : "4",
    "PacketDest.Normal.Sigma" : "2",
    "PacketDest.NearestNeighbor.Size" : "3 3 1",

    "PacketSize.pattern" : "Normal",
    "PacketSize.RangeMin" : "32", #4B, integer for bits, not UnitAlgebra
    "PacketSize.RangeMax" : "256", #32B, integer for bits, not UnitAlgebra
    "PacketSize.HotSpot.target" : "224",
    "PacketSize.HotSpot.targetProbability" : ".99",
    "PacketSize.Normal.Mean" : "96",
    "PacketSize.Normal.Sigma" : "16",

    "PacketDelay.pattern" : "Uniform",
    "PacketDelay.RangeMax" : "20",
}

OfferedLoadParams = {
    "link_bw" : noc_link_bw,
    "linkcontrol" : "mordred.mordredNIC",
    "buffer_size" : "1kiB",
    "packet_size" : "8B",
    "pattern" : "merlin.targetgen.uniform",
    "offered_load" : "0.2",
    "warmup_time" : "1us",
    "collect_time" : "50us",
    "drain_time" : "50us"
}


#EOF
