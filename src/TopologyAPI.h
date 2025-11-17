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
#include <queue>

// Local SST header
#include "sst_config.h"

/*
 * In Merlin, the Topology API is in router.h; the API generally maintains a set of routing
 * functions and an enum tracking what each port of the router is connected to;
 * a subset of the routing functions is processing and handling untimed functions
 */

namespace SST::Mordred {

class TopologyAPI : public SubComponent {
public:
  SST_ELI_REGISTER_SUBCOMPONENT_API( SST::Mordred::TopologyAPI, ComponentId_t, uint32_t, uint32_t, std::vector<uint32_t>* )

  /// TopologyAPI: constructor
  TopologyAPI( ComponentId_t id ) : SubComponent( id ) {}

  /// TopologyAPI: constructor
  TopologyAPI() : SubComponent() {}

  /// TopologyAPI: default destructor
  ~TopologyAPI() override  = default;

  /// Computed endpoint ID
  virtual int32_t getEndpointId( uint32_t portnum ) = 0;

  /// Get the output port for a packet
  virtual uint32_t routePacket( uint32_t dest ) = 0;

  /// Do routing for untimed broadcast packets
  virtual void routeUntimedBroadcastPacket( Event* ev, std::queue<Event>& output_events ) = 0;

};  // class TopologyAPI

}  // namespace SST::Mordred

#endif //TOPOLOGYAPI_H
