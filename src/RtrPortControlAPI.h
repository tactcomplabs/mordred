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
 * with no inheritance from SimpleNetwork.  That written, the PortInterface has many of
 * the same functions as a SimpleNetwork::Interface.
 *
 */

namespace SST::Mordred {

class InVcHeads; // forward declaration
class TopologyAPI; // forward declaration

// Not using the InVcStateE right now
enum InVcStateE { IN_IDLE, ROUTING, WAIT_VC, WAIT_CREDITS, IN_ACTIVE }; // for the port input side - match Dally book
enum OutVcStateE { OUT_IDLE, OUT_BUSY, NEED_CREDITS}; // for the port output side - mainly for xbar arb

/**
 * For now, I'm taking the following two structures pretty much directly
 * from the Dally interconnections book
 *
 * The buffers in these structs are not size limited; we'll use the number of credits
 * to manage the usage of the collected set of buffers
 *
 * TODO: probably need to provide functions to allow for updates (or at least an update
 * mechanism)
 *
 * TODO: Make the port, vn, and vc parameters all uint32_t's (at some point we can change them all to
 * uint16_t's)
 */
struct perVcInState {
  InVcStateE inVcState;
  uint16_t outPort;
  uint16_t outVn;
  uint16_t outVc;
  int16_t availOutCredits; // captures available credits for outPort.outVn.outVc -- may not need - might want to just check on each cycle
  int16_t retCredits; // credits to return to sender as inBuf is emptied; init to zero
  std::queue<MordredFlit*> inBuf;

  void reset() {
    inVcState = IN_IDLE;
    outPort = UINT16_MAX;
    outVn = UINT16_MAX;
    outVc = UINT16_MAX;
    availOutCredits = 0;
    retCredits = 0;
  }
};

struct perVcOutState {
  OutVcStateE outVcState;
  uint16_t inPort;
  uint16_t inVn;
  uint16_t inVc;
  int16_t outBufCredits; // space available in outBuf; dec on write to buf, inc when flit put on link
  // (dec on send to dest, inc when credit packet comes from dest)
  int16_t destCredits; // maintains available credits for myVn.myVc downstream; initialized to non-zero in init (dest sends a count)
  std::queue<MordredFlit*> outBuf;

  void reset( int16_t ob_creds ) {
    outVcState = OUT_IDLE;
    inPort = UINT16_MAX;
    inVn = UINT16_MAX;
    inVc = UINT16_MAX;
    outBufCredits = ob_creds;
    destCredits = 0;
  }
};

class RtrPortControlAPI : public SubComponent {
public:
  SST_ELI_REGISTER_SUBCOMPONENT_API( SST::Mordred::RtrPortControlAPI, TopologyAPI*,
    RtrOwnedSharedObjs*, uint32_t, uint32_t )

  enum PortConnectionE { ENDPOINT, ROUTER, UNKNOWN, INVALID };

  /// RtrPortControlAPI: constructor
  RtrPortControlAPI( ComponentId_t id ) : SubComponent( id ) {}

  /// RtrPortControlAPI: default destructor
  ~RtrPortControlAPI() override = default;

  /// Untimed recv/send
  virtual void sendUntimedData(Event *ev) = 0;
  virtual Event* recvUntimedData() = 0;

  // No separate clock - run off the router clock
  // TODO: This may get broken up into separate update and execute functions; booksim approached their
  // model this way and described as update being the combinational logic and execute being the state
  // updates
  virtual void ClockTick( Cycle_t cycle ) = 0;

  virtual int32_t getOutBufCreditCount( std::pair<uint32_t, uint32_t> vn_vc ) = 0;

  // VC Allocator functions
  virtual uint32_t getOutPort( uint32_t vn, uint32_t vc ) = 0;
  virtual uint32_t assignOutVc( uint32_t vn, uint32_t start_vc ) = 0;
  virtual void inUnitSetOutputVc( uint32_t vn, uint32_t input_vc, uint32_t output_vc ) = 0;
  virtual void outUnitSetInputVc( uint32_t vn, uint32_t input_vc, uint32_t output_vc ) = 0;

  // Get state for a VC
  // Intended to mark the status of a port for the xbar arbitration
  //virtual InVcStateE getInVcState( uint32_t vc ) = 0;
  //virtual OutVcStateE getOutVcState( uint32_t vc ) = 0;

  // TODO: Do I really need to be using pairs?
  virtual MordredFlit* getInBufFlit( std::pair<uint32_t, uint32_t> vn_vc ) = 0;
  virtual void   recvOutBufFlit( MordredFlit* flit, std::pair<uint32_t, uint32_t> vn_vc )  = 0;

};  // class RtrPortControlAPI

}  // namespace SST::Mordred

#endif //RTRPORTCONTROLAPI_H
