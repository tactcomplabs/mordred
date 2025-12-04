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
clk = UnitAlgebra("2200MHz")
clk_pd = clk.invert()
link_latency = UnitAlgebra(0.8) * clk_pd
data_flit_size = UnitAlgebra("36B")
ctrl_flit_size = UnitAlgebra("8B")
num_vns = "1"
noc_link_bw = clk * data_flit_size

BridgeRtrParams = {
    "verbose" : "5",
    "clock" : UnitAlgebra("1000MHz"),
    "num_vcs" : "1",
    "num_vns" : 1,
    "flit_size" : "80B",
    "input_buf_size" : UnitAlgebra(8)*UnitAlgebra("80B"),
    "output_buf_size" : UnitAlgebra(1)*UnitAlgebra("80B")
}

DataRtrParams = {
    "verbose" : "0",
    "clock" : clk,
    "num_vcs" : "1",
    "num_vns" : num_vns,
    "flit_size" : data_flit_size,
    "input_buf_size" : UnitAlgebra(8)*data_flit_size,
    "output_buf_size" : UnitAlgebra(1)*data_flit_size
}

CtrlRtrParams = {
    "verbose" : "0",
    "clock" : clk,
    "num_vcs" : "1",
    "num_vns" : num_vns,
    "flit_size" : ctrl_flit_size,
    "input_buf_size" : UnitAlgebra(4)*ctrl_flit_size,
    "output_buf_size" : UnitAlgebra(1)*ctrl_flit_size
}

FixedTestNicParams = {
    "num_messages" : 10,
    "message_size" : UnitAlgebra(4)*data_flit_size,
    "send_untimed_broadcast" : "false", # matches default
    #"send_untimed_broadcast" : "true",
}

MordredNICParams = {
    "verbose" : "5",
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
    "packets_to_send" : "20",
    "packet_size" : "16B", # Seems to dominate over the PacketSize.Range{Min,Max} parameters
    "delay_between_packets" : "10ns",
    "message_rate" : "1GHz"
}

TrafficGenMessagingParams = {
    "PacketDest.pattern" : "Uniform",
    # "PacketDest.RangeMax" -- set to num_peers (when defining the system)
    "PacketSize.pattern" : "Uniform",
    "PacketSize.RangeMin" : "4B",
    "PacketSize.RangeMax" : "32B",
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
