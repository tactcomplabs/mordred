//
// FlatButterflyTopo.h
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef FLATBUTTERFLYTOPO_H
#define FLATBUTTERFLYTOPO_H

// Standard headers
#include <cstdint>

// Local SST header
#include "sst_config.h"

// Local headers
#include "TopologyAPI.h"
#include "MordredEvents.h"

namespace SST::Mordred {

class FlatButterflyTopo : public TopologyAPI {

public:
  // register with the SST Core
  SST_ELI_REGISTER_SUBCOMPONENT(
    FlatButterflyTopo,
    "mordred",       // component library
    "flattenedButterfly",  // component name
    SST_ELI_ELEMENT_VERSION( 0, 0, 1 ),
    "Flattened Butterfly Topology for NoC Router",
    SST::Mordred::TopologyAPI
  )

  // register the parameters
  SST_ELI_DOCUMENT_PARAMS(
    { "verbose", "Sets the output verbsoity", "5" },
    //{ "xDim", "Number of points in the X dimension", "1"},
    // {"yDim", "Number of points in the Y dimension", "1"}
  )

  // register the ports
  SST_ELI_DOCUMENT_PORTS()

  // register the subcomponent slots
  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS()

  /// FlatButterflyTopo: constructor
  FlatButterflyTopo( ComponentId_t id, Params& params, uint32_t rtr_id, uint32_t num_ports, uint32_t num_local_ports );

  /// FlatButterflyTopo: destructor
  ~FlatButterflyTopo() override = default;

  int32_t getEndpointId( uint32_t portnum ) override;

  /// Get the output port for a flit
  uint32_t routePacket( uint32_t dest ) final;


private:
  Output* output;

  uint32_t rtrId;
  uint32_t numPorts;
  uint32_t numLocalPorts;

};

} // namespace SST::Mordred


#endif //FLATBUTTERFLYTOPO_H
