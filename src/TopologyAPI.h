//
// TopologyAPI.h
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef TOPOLOGYAPI_H
#define TOPOLOGYAPI_H

// Standard headers
#include <cstdint>

// Local SST header
#include "sst_config.h"

// Other local headers
#include "MordredEvents.h"

namespace SST {
namespace Mordred {

class TopologyAPI : public SST::SubComponent {
public:
  SST_ELI_REGISTER_SUBCOMPONENT_API( SST::Mordred::TopologyAPI )

  /// TopologyAPI: constructor
  TopologyAPI( ComponentId_t id, Params& params ) : SubComponent( id ) {}

  /// TopologyAPI: default destructor
  ~TopologyAPI() override                                    = default;

  /// TopologyAPI: prep and/or send an initialization message
  virtual MordredFlit* sendInitMessage()                     = 0;

  /// TopologyAPI: receive and handle an initialization message
  /// TODO: Include phase?
  virtual void processInitMessage( size_t topo_port_num, Event *ev ) = 0;

};  // class TopologyAPI

} // namespace Mordred
} // namespace SST

#endif //TOPOLOGYAPI_H
