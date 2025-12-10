//
// RtrPortControl.h
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//
//

#ifndef RTRPORTCONTROL_H
#define RTRPORTCONTROL_H

// Local SST config
#include "sst_config.h"

#include "MordredEvents.h"
#include "RtrPortControlAPI.h"

namespace SST::Mordred {


/**
 * Have a per VN,VC structure in RtrPortControlAPI that mostly models an input or output
 * unit in the Dally book.  It's owned here and we'll pass along the reference to the port
 * vector to whichever units need it from the router
 *
 * For now, let's do routing when we get a head flit at the head of a queue;
 * we're going to assume
 * that we have enough logic so that we don't have to worry about arbitrating
 * access to the router itself
 *
 */

/**
 * Miscellaneous Notes:
 *  - If this is an ENDPOINT port, it really only needs one VC; we don't pass that knowledge into
 *    the object and configure it that way though (would be pretty easy since we're passing in the
 *    full params struct)
 *  - Only connected ports should ever be created (via the SimpleRtr constructor) so we don't
 *      do any checking for that here
 *  - For a given clock tick, if all of the output buffers are empty/blocked from sending,
 *      then this module will try to return credits using a round-robin approach.
 */

class RtrPortControl : public RtrPortControlAPI {
public:
  SST_ELI_REGISTER_SUBCOMPONENT(
    RtrPortControl,
    "mordred",
    "rtrPortControl",
    SST_ELI_ELEMENT_VERSION( 0, 1, 0 ),
    "Manage a port on the Mordred router",
    SST::Mordred::RtrPortControlAPI
  )

  // All of the parameters are handled/passed in from the SimpleRtr
  SST_ELI_DOCUMENT_PARAMS(
//    { "verbose", "Sets the output verbsoity", "5" },
    )
  // For reference, params from SimpleRtr:
  // verbose, flit_size, channel_width, input_buf_size, output_buf_size,

  // Use the parent ports -- assume anonymous loading
  SST_ELI_DOCUMENT_PORTS()

  SST_ELI_DOCUMENT_STATISTICS(
    // These are pretty useless unless I know if that is going to link or internal
  {"recv_flit_cnt", "Number of flits received on the link", "unitless", 3},
  {"sent_flit_cnt", "Number of flits sent on the link", "unitless", 3},
  {"sent_packet_cnt", "Number of packets sent on the link", "unitless", 3},
  {"output_stalls", "Number of cycles stalled on sending output", "unitless", 3}
  )

  RtrPortControl( ComponentId_t id, Params& params, TopologyAPI* topology, RtrOwnedSharedObjs* rtr_shared_objs, uint32_t rtr_num, uint32_t port_num );

  ~RtrPortControl() final = default;

  // Lifecycle functions
  void init(unsigned int phase) final;
  void setup() final;
  void complete(unsigned int phase) final;
  void finish() override { /* empty */ }

  void sendUntimedData(Event* ev) final;
  Event* recvUntimedData() final;

  PortConnectionE getConnectionType() final { return connectionType; }
  uint32_t getPortId() final { return portId; }

  void ClockTick(Cycle_t cycle) final;

  void inHandler(Event* ev);

  void validateVnVc( uint32_t vn, uint32_t vc ) {
    if ( (vn == UINT32_MAX) || (vc == UINT32_MAX) )
      output->fatal( CALL_INFO, -1, "Invalid vn=%u or vc=%u\n", vn, vc );
    if ( vn >= numVns )
      output->fatal( CALL_INFO, -1, "Invalid vn=%u \n", vn );
    if ( vc >= numVcs )
      output->fatal( CALL_INFO, -1, "Invalid vc=%u \n", vc );
  }

  // VC Alloc interactions
  std::pair<uint32_t, uint32_t> getSwitchSendVnVc() final {
    return std::tuple(switch_alloc_sendfrom_vn, switch_alloc_sendfrom_vc );
  }

  std::pair<uint32_t, uint32_t> getSwitchRecvVnVc() final {
    return std::tuple(switch_alloc_rcvto_vn, switch_alloc_rcvto_vc );
  }

  uint32_t getDestPort( uint32_t vn, uint32_t vc ) final {
    validateVnVc( vn, vc );
    return inStateVec.at(vn).at(vc).outPort;
  }

  OutVcStateE getOutputState( uint32_t vn, uint32_t vc ) final {
    validateVnVc( vn, vc );
    return outStateVec.at(vn).at(vc).outVcState;
  }

  void inUnitSetDestVc( uint32_t vn, uint32_t input_vc, uint32_t dest_vc ) final {
    validateVnVc( vn, input_vc );
    // TODO: Where called from? Are these ever reset?
    // These probably ought to be reset when we see a tail flit
    // Looks like we reset in RtrPortControl::resetPerVcDest()
    inStateVec.at(vn).at(input_vc).outVn = vn;
    inStateVec.at(vn).at(input_vc).outVc = dest_vc;
  }

  void outUnitSetSrc( uint32_t port, uint32_t vn, uint32_t src_vc, uint32_t output_vc ) final {
    validateVnVc( vn, output_vc );
    outStateVec.at(vn).at(output_vc).outVcState = OUT_BUSY;
    outStateVec.at(vn).at(output_vc).inPort = port;
    outStateVec.at(vn).at(output_vc).inVn = vn;
    outStateVec.at(vn).at(output_vc).inVc = src_vc;
  }

  // Switch allocation functions
  bool isSendAllocatedToSwitch() final {
    // Might want to check switch_alloc_vc as well
    if( switch_alloc_sendto_port != UINT32_MAX )
      return true;
    return false;
  }

  void sendAllocateToSwitch( uint32_t port, uint32_t vn, uint32_t vc ) final {
    validateVnVc( vn, vc );
    if ( port != inStateVec.at(vn).at(vc).outPort )
      output->fatal( CALL_INFO, -1, "Port mismatch in=%" PRIu32 ", state=%" PRIu32 "\n",
        port, inStateVec.at(vn).at(vc).outPort );
    switch_alloc_sendto_port = port;
    switch_alloc_sendfrom_vn = vn;
    switch_alloc_sendfrom_vc = vc;
    //output->verbose( CALL_INFO, 5, 0, "SendTo Port=%u; send vn,vc=[%u,%u]\n",
    //  port, vn, vc);
    //output->flush();
  }
  void resetSwitchSendAllocation() final {
    validateVnVc( switch_alloc_sendfrom_vn, switch_alloc_sendfrom_vc );
    inStateVec.at( switch_alloc_sendfrom_vn ).at(switch_alloc_sendfrom_vc).inVcState = IN_IDLE;
    switch_alloc_sendto_port = switch_alloc_sendfrom_vn = switch_alloc_sendfrom_vc = UINT32_MAX;
  }

  bool isRecvAllocatedFromSwitch() final {
    if ( switch_alloc_rcvto_vn != UINT32_MAX )
      return true;
    return false;
  }
  void recvAllocateFromSwitch( uint32_t sending_port, uint32_t vn, uint32_t vc ) final {
    validateVnVc( vn, vc );
    if ( sending_port != outStateVec.at(vn).at(vc).inPort )
      output->fatal( CALL_INFO, -1, "Port mismatch in=%" PRIu32 ", state=%" PRIu32 "\n",
        sending_port, outStateVec.at(vn).at(vc).inPort );
    switch_alloc_rcvfrom_port = sending_port;
    switch_alloc_rcvto_vn = vn;
    switch_alloc_rcvto_vc = vc;
    //output->verbose( CALL_INFO, 5, 0, "RcvFrom Port=%u; recv vn,vc=[%u,%u]\n",
    //  sending_port, vn, vc);
    //output->flush();
  }
  void resetSwitchRecvAllocation() final {
    validateVnVc( switch_alloc_rcvto_vn, switch_alloc_rcvto_vc );
    outStateVec.at( switch_alloc_rcvto_vn ).at(switch_alloc_rcvto_vc ).outVcState = OUT_IDLE;
    switch_alloc_rcvfrom_port = switch_alloc_rcvto_vn = switch_alloc_rcvto_vc = UINT32_MAX;
  }
  uint32_t getDestVc( uint32_t vn, uint32_t vc ) final {
    validateVnVc( vn, vc );
    return inStateVec.at(vn).at(vc).outVc; // TODO: Where does this get set? RtrPortControl::inUnitSetDestVc
  }

  // Switch/xbar interactions
  // Return UINT32_MAX for idle
  // Return UINT32_MAX-1 if blocked (no credits)

  uint32_t getSendingPort() final {
    if ( switch_alloc_rcvfrom_port == UINT32_MAX ) // no sender
      return UINT32_MAX;
    validateVnVc( switch_alloc_rcvto_vn, switch_alloc_rcvto_vc );
    if ( outStateVec.at(switch_alloc_rcvto_vn).at(switch_alloc_rcvto_vc).outBufCredits > 0 )
      return switch_alloc_rcvfrom_port;
    return UINT32_MAX-1;
  }

  MordredFlit* getInBufFlit() final;
  void   recvOutBufFlit( MordredFlit* flit ) final; // Rename?

  void resetPerVcDest() final {
    validateVnVc( switch_alloc_sendfrom_vn, switch_alloc_sendfrom_vc );
    // This should be resetting the out{port,vn,vc} in an input unit - use the switch_alloc_sendfrom parameters
    inStateVec.at(switch_alloc_sendfrom_vn).at(switch_alloc_sendfrom_vc).outPort =
      inStateVec.at(switch_alloc_sendfrom_vn).at(switch_alloc_sendfrom_vc).outVn =
        inStateVec.at(switch_alloc_sendfrom_vn).at(switch_alloc_sendfrom_vc).outVc = UINT32_MAX;
  }

  void resetPerVcSrc() final {
    validateVnVc( switch_alloc_rcvto_vn, switch_alloc_rcvto_vc );
    outStateVec.at(switch_alloc_rcvto_vn).at(switch_alloc_rcvto_vc).inPort =
      outStateVec.at(switch_alloc_rcvto_vn).at(switch_alloc_rcvto_vc).inVn =
        outStateVec.at(switch_alloc_rcvto_vn).at(switch_alloc_rcvto_vc).inVc = UINT32_MAX;
  }

  uint32_t getConnectedRtrId() const final { return connectedRtrId; }

  /// default constructor
  RtrPortControl() : RtrPortControlAPI() {}

  /// serialization
  void serialize_order(SST::Core::Serialization::serializer& ser) override {
    SST_SER(output);
    SST_SER(link);
    SST_SER(topo);
    SST_SER(connectionType);
    SST_SER(rtrId);
    SST_SER(portId);
    SST_SER(connectedRtrId);
    SST_SER(connectedPortId);
    SST_SER(numVns);
    SST_SER(numVcs);
    SST_SER(flitSize);
    SST_SER(flit_vn_rr);
    SST_SER(flit_vc_rr);
    SST_SER(credit_ret_vn_rr);
    SST_SER(credit_ret_vc_rr);
    SST_SER(switch_alloc_sendto_port);
    SST_SER(switch_alloc_sendfrom_vn);
    SST_SER(switch_alloc_sendfrom_vc);
    SST_SER(switch_alloc_rcvfrom_port);
    SST_SER(switch_alloc_rcvto_vn);
    SST_SER(switch_alloc_rcvto_vc);
    SST_SER(param_link_bw);
    SST_SER(param_flit_size);
    SST_SER(inBufSize);
    SST_SER(outBufSize);
    SST_SER(initEvents);
    SST_SER(inStateVec);
    SST_SER(outStateVec);
    SST_SER(rtrSharedObjs);
    SST_SER(statLinkRecvFlitCnt);
    SST_SER(statLinkSentFlitCnt);
    SST_SER(statLinkSentPacketCnt);
    SST_SER(statLinkOutputStalledCnt);
  }

  /// serialization implementations
  ImplementSerializable(SST::Mordred::RtrPortControl);

private:
  void allocateBuffers(); // this also registers the stats
  MordredInitEvent* getInitEvent( MordredInitEvent::Commands cmd );
  void returnCredit();

  Output* output;
  Link*   link{};
  TopologyAPI *topo{};
  PortConnectionE connectionType{UNKNOWN};
  uint32_t rtrId;
  uint32_t portId;
  uint32_t connectedRtrId{UINT32_MAX}; // UINT32_MAX - 1 denotes an active endpoint
  uint32_t connectedPortId{UINT32_MAX};
  uint32_t numVns{UINT32_MAX}; // from size of vn_objs
  uint32_t numVcs{UINT32_MAX}; // from size of vector in vn_objs
  uint32_t flitSize{}; // in bits
  uint32_t flit_vn_rr{};
  uint32_t flit_vc_rr{};
  uint32_t credit_ret_vn_rr{};
  uint32_t credit_ret_vc_rr{};

  // For switch allocation
  // sendto == destination info; rcvfrom == src info
  // the sendto terms should match what's in the perVcInState struct
  // similarly, the rcvfrom terms should match what's in the perVcOutState struct

  // This lets us get info from the sender to query the receiver; mainly needed for credits
  uint32_t switch_alloc_sendto_port{UINT32_MAX};
  uint32_t switch_alloc_sendfrom_vn{UINT32_MAX};
  uint32_t switch_alloc_sendfrom_vc{UINT32_MAX};

  uint32_t switch_alloc_rcvfrom_port{UINT32_MAX};
  uint32_t switch_alloc_rcvto_vn{UINT32_MAX};
  uint32_t switch_alloc_rcvto_vc{UINT32_MAX};

  UnitAlgebra param_link_bw;
  UnitAlgebra param_flit_size;

  // These are in bits
  uint32_t inBufSize;
  uint32_t outBufSize;

  std::queue<Event*> initEvents;

  // These are 2D - [vn][vc]
  std::vector<std::vector<perVcInState>> inStateVec;
  std::vector<std::vector<perVcOutState>> outStateVec;

  // This holds the element for this specific port (SimpleRtr owns it)
  RtrOwnedSharedObjs *rtrSharedObjs{};

  // Statistics
  // For the 2D vectors, the outer dimension is VN, inner is VC
  // MUST have this parameter set (ComponentInfo::INSERT_STATS) as part of the
  // share_flags when loading the subcomponent.  SST docs don't seem to describe
  // the share_flags anywhere
  std::vector<std::vector<Statistic<uint64_t>*>> statLinkRecvFlitCnt;
  std::vector<std::vector<Statistic<uint64_t>*>> statLinkSentFlitCnt;
  std::vector<std::vector<Statistic<uint64_t>*>> statLinkSentPacketCnt;
  Statistic<uint64_t>* statLinkOutputStalledCnt{}; // TODO: Currently unused; see comments in ClockTick()
};

} // namespace SST::Mordred
#endif //RTRPORTCONTROL_H
