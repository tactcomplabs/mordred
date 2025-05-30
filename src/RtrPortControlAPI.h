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
//#include "TopologyAPI.h"
//#include "SharedStructs.h"

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

class InVcHeads; // forward declaration
class TopologyAPI; // forward declaration

class RtrPortControlAPI : public SubComponent {
public:
  SST_ELI_REGISTER_SUBCOMPONENT_API( SST::Mordred::RtrPortControlAPI, TopologyAPI*,
    std::vector<RtrOwnedVnObj>*, uint32_t, uint32_t )

  enum PortConnectionE { ENDPOINT, ROUTER, UNKNOWN, INVALID };
  // Not using the InVcStateE right now
  enum InVcStateE { IN_IDLE, ROUTING, WAIT_OUTPUT, IN_BUSY }; // for the port input side - mainly for xbar arb
  enum OutVcStateE { OUT_IDLE, OUT_BUSY, NEED_CREDITS}; // for the port output side - mainly for xbar arb

  /// RtrPortControlAPI: constructor
  RtrPortControlAPI( ComponentId_t id ) : SubComponent( id ) {}

  /// RtrPortControlAPI: default destructor
  ~RtrPortControlAPI() override = default;

  /// Untimed recv/send
  virtual void sendUntimedData(Event *ev) = 0;
  virtual Event* recvUntimedData() = 0;

  // No separate clock - run off the router clock
  virtual void ClockTick( Cycle_t cycle ) = 0;

  virtual int32_t getOutBufCreditCount( std::pair<uint32_t, uint32_t> vn_vc ) = 0;

  // Get state for a VC
  // Intended to mark the status of a port for the xbar arbitration
  //virtual InVcStateE getInVcState( uint32_t vc ) = 0;
  //virtual OutVcStateE getOutVcState( uint32_t vc ) = 0;

  virtual MordredFlit* getInBufFlit( std::pair<uint32_t, uint32_t> vn_vc ) = 0;
  virtual void   sendOutBufFlit( MordredFlit* flit, std::pair<uint32_t, uint32_t> vn_vc )  = 0;

};  // class RtrPortControlAPI

}  // namespace SST::Mordred

#endif //RTRPORTCONTROLAPI_H
