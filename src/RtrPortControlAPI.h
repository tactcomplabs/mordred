//
// RtrPortControlAPI.h
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef RTRPORTCONTROLAPI_H
#define RTRPORTCONTROLAPI_H

// Local SST config
#include <cstdint>

#include "sst_config.h"

#include "MordredEvents.h"
#include "TopologyAPI.h"

/*
 * In Merlin, the PortInterface API is in router.h; the purpose of the PortInterface is to
 * provide a set of functions for sending/receiving messages on one of the ports of the
 * router. This is separate from the linkControl (in Merlin) and MordredNIC (here) that
 * is for the endpoint connecting to the network.
 *
 * In Merlin::PortInterface, there is also the API for the output arbitration unit;
 * based on the output of sst-info, there are at least a couple of options for this
 * Default is merlin.arb.output.basic
 *
 * While the MordredNIC inherits from SimpleNetwork::Interface (as does Merlin::linkControl),
 * the Merlin::PortInterface and Merlin::PortControl are a subcomponent API and subcomponent
 * with no inheritance from SimpleNetwork.  That written, the PortInterface has many (all?) of
 * the same functions as a SimpleNetwork::Interface.
 *
 */

namespace SST::Mordred {

class RtrPortControlAPI : public SubComponent {
public:
  SST_ELI_REGISTER_SUBCOMPONENT_API( SST::Mordred::RtrPortControlAPI, TopologyAPI*, uint32_t, uint32_t )


  /// RtrPortControlAPI: constructor
  RtrPortControlAPI( ComponentId_t id ) : SubComponent( id ) {}

  /// RtrPortControlAPI: default destructor
  ~RtrPortControlAPI() override = default;

  /// Untimed recv/send
  virtual void sendUntimedData(Event *ev) = 0;
  virtual Event* recvUntimedData() = 0;

};  // class RtrPortControlAPI

}  // namespace SST::Mordred

#endif //RTRPORTCONTROLAPI_H
