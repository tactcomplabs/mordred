#!/usr/bin/env python
#
# Copyright 2009-2025 NTESS. Under the terms
# of Contract DE-NA0003525 with NTESS, the U.S.
# Government retains certain rights in this software.
#
# Copyright (c) 2009-2025, NTESS
# All rights reserved.
#
# This file is part of the SST software package. For license
# information, see the LICENSE file in the top level directory of the
# distribution.

import sst

# from merlin_testing.torus_5_trafficgen import endPoint

sst.setProgramOption("stop-at", "1ms")

load_level = 80
load_factor = (load_level/100)

sst.setStatisticLoadLevel(7)
sst.setStatisticOutput("sst.statOutputCSV", {"filepath" : "./merlin.COL.LF%s.csv"%load_level,"separator" : "," } )
#sst.setStatisticOutput("sst.statOutputCSV", {"filepath" : "./stats.TGEN.csv","separator" : "," } )

from sst.merlin import *

sst.merlin._params["flit_size"] = "2B"
sst.merlin._params["link_bw"] = "1.0GB/s"
sst.merlin._params["xbar_bw"] = "1.0GB/s"
sst.merlin._params["input_latency"] = "50ps"
sst.merlin._params["output_latency"] = "50ps"
sst.merlin._params["input_buf_size"] = "32B"
sst.merlin._params["output_buf_size"] = "32B"
#sst.merlin._params["link_lat"] = "5000ns"

merlinemeshparams = {}
#merlinemeshparams["num_dims"]=2
merlinemeshparams["mesh.shape"]="3x3"
merlinemeshparams["mesh.width"]="1x1"
merlinemeshparams["mesh.local_ports"]=1
sst.merlin._params.update(merlinemeshparams)
topo = topoMesh()
topo.prepParams()

sst.merlin._params["PacketDest.pattern"] = "Uniform"
sst.merlin._params["PacketDest.RangeMin"] = "0.0"
sst.merlin._params["PacketDest.RangeMax"] = "8.0"
sst.merlin._params["PacketSize.pattern"] = "Uniform"
sst.merlin._params["PacketSize.RangeMin"] = "64b"
sst.merlin._params["PacketSize.RangeMax"] = "64b"
# Required by pymerlin
sst.merlin._params["packet_size"] = "8B"
sst.merlin._params["PacketDelay.pattern"] = "Uniform"
sst.merlin._params["PacketDelay.RangeMin"] = "5.0ns"
sst.merlin._params["PacketDelay.RangeMax"] = "10.0ns"
# Required by pymerlin
sst.merlin._params["message_rate"] = "1GHz"
sst.merlin._params["packets_to_send"] = 10

# Offered Load Params
sst.merlin._params["offered_load"] = load_factor
# sst.merlin._params["num_peers"] = "9" # uncommenting this crashes the script
sst.merlin._params["message_size"] = "8B"

## From interactive screen
sst.merlin._params["link_lat"] = "700ps"
sst.merlin._params["buffer_size"] = "1KiB"
sst.merlin._params["pattern"] = "merlin.targetgen.uniform"
sst.merlin._params["warmup_time"] = "1us" # critical requirement
sst.merlin._params["collect_time"] = "500us" # not sure why this got ignored
#sst.merlin._params["drain_time"] = "50us"

# For clocked offered load
sst.merlin._params["clock_rate"] = "1GHz"

#endPoint = TrafficGenEndPoint()
#endPoint.prepParams()

#endPoint = OfferedLoadEndPoint()
#endPoint.prepParams()

endPoint = ClockedOfferedLoadEndPoint()
endPoint.prepParams()

topo.setEndPoint(endPoint)
topo.build()

sst.enableAllStatisticsForAllComponents({"type":"sst.AccumulatorStatistic","rate":"0ns"})
