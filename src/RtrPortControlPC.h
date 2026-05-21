//
// RtrPortControlPC.h
//
// Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef MORDRED_RTRPORTCONTROLPC_H
#define MORDRED_RTRPORTCONTROLPC_H

// Local SST config
#include "sst_config.h"

#include "MordredEvents.h"
#include "PhysChannelAPI.h"
#include "RtrPortControlAPI.h"

namespace SST::Mordred {

/**
 * SimpleNetwork-backed implementation of RtrPortControlAPI.
 *
 * Instead of owning an SST::Link directly, this class loads a user-provided
 * SST::Interfaces::SimpleNetwork subcomponent (slot "port_iface") to manage
 * the physical link.  Both ends of the link must use a matching SimpleNetwork
 * type (e.g. UCIeInterfaceSN), making the physical interconnect pluggable
 * while the router crossbar/VC/credit machinery is unchanged.
 *
 * VN mapping: SN VN i == Mordred VN i.  Both MordredFlit and MordredCreditEvent
 * for Mordred VN i are sent on SN VN i; the receiver dispatches on payload type.
 *
 * Port name convention: the constructor inserts "port_name" = "portN" into the
 * params forwarded to the SN subcomponent so it calls configureLink("portN").
 * The SN implementation must support the "port_name" parameter.
 */
class RtrPortControlPC : public RtrPortControlAPI {
public:
  SST_ELI_REGISTER_SUBCOMPONENT(
    RtrPortControlPC,
    "mordred",
    "rtrPortControlPC",
    SST_ELI_ELEMENT_VERSION( 0, 1, 0 ),
    "SimpleNetwork-backed port control for the Mordred router",
    SST::Mordred::RtrPortControlAPI
  )

  SST_ELI_DOCUMENT_PARAMS(
    // Inherits params from SimpleRTR: verbose, flit_size, input_buf_size, output_buf_size
  )

  // Uses parent ports via SHARE_PORTS; the SN subcomponent configures the actual link
  SST_ELI_DOCUMENT_PORTS()

  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
    { "port_iface", "PhysChannelAPI subcomponent that manages the physical port link", "SST::Mordred::PhysChannelAPI" }
  )

  SST_ELI_DOCUMENT_STATISTICS(
    { "recv_flit_cnt", "Number of flits received on the link", "unitless", 3 },
    { "sent_flit_cnt", "Number of flits sent on the link", "unitless", 3 },
    { "sent_packet_cnt", "Number of packets sent on the link", "unitless", 3 },
    { "output_stalls", "Number of cycles stalled on output", "unitless", 3 }
  )

  RtrPortControlPC(
    ComponentId_t       id,
    Params&             params,
    TopologyAPI*        topology,
    RtrOwnedSharedObjs* rtr_shared_objs,
    uint32_t            rtr_num,
    uint32_t            port_num
  );

  ~RtrPortControlPC() final = default;

  // Lifecycle
  void init( unsigned int phase ) final;
  void setup() final;
  void complete( unsigned int phase ) final;

  void finish() override { /* empty */ }

  void   sendUntimedData( Event* ev ) final;
  Event* recvUntimedData() final;

  PortConnectionE getConnectionType() final { return connectionType; }

  uint32_t getPortId() final { return portId; }

  void ClockTick( Cycle_t cycle ) final;

  // Receive notification callback registered with the SN subcomponent
  bool onReceive( int sn_vn );

  void validateVnVc( uint32_t vn, uint32_t vc ) {
    if( ( vn == UINT32_MAX ) || ( vc == UINT32_MAX ) )
      output->fatal( CALL_INFO, -1, "Invalid vn=%u or vc=%u\n", vn, vc );
    if( vn >= numVns )
      output->fatal( CALL_INFO, -1, "Invalid vn=%u\n", vn );
    if( vc >= numVcs )
      output->fatal( CALL_INFO, -1, "Invalid vc=%u\n", vc );
  }

  // VC Allocator interactions
  std::pair<uint32_t, uint32_t> getSwitchSendVnVc() final {
    return std::pair( switch_alloc_sendfrom_vn, switch_alloc_sendfrom_vc );
  }

  std::pair<uint32_t, uint32_t> getSwitchRecvVnVc() final { return std::pair( switch_alloc_rcvto_vn, switch_alloc_rcvto_vc ); }

  uint32_t getDestPort( uint32_t vn, uint32_t vc ) final {
    validateVnVc( vn, vc );
    return inStateVec.at( vn ).at( vc ).outPort;
  }

  OutVcStateE getOutputState( uint32_t vn, uint32_t vc ) final {
    validateVnVc( vn, vc );
    return outStateVec.at( vn ).at( vc ).outVcState;
  }

  void inUnitSetDestVc( uint32_t vn, uint32_t input_vc, uint32_t dest_vc ) final {
    validateVnVc( vn, input_vc );
    inStateVec.at( vn ).at( input_vc ).outVn = vn;
    inStateVec.at( vn ).at( input_vc ).outVc = dest_vc;
  }

  void outUnitSetSrc( uint32_t port, uint32_t vn, uint32_t src_vc, uint32_t output_vc ) final {
    validateVnVc( vn, output_vc );
    outStateVec.at( vn ).at( output_vc ).outVcState = OUT_BUSY;
    outStateVec.at( vn ).at( output_vc ).inPort     = port;
    outStateVec.at( vn ).at( output_vc ).inVn       = vn;
    outStateVec.at( vn ).at( output_vc ).inVc       = src_vc;
  }

  // Switch allocation functions
  bool isSendAllocatedToSwitch() final { return switch_alloc_sendto_port != UINT32_MAX; }

  void sendAllocateToSwitch( uint32_t port, uint32_t vn, uint32_t vc ) final {
    validateVnVc( vn, vc );
    if( port != inStateVec.at( vn ).at( vc ).outPort )
      output->fatal(
        CALL_INFO, -1, "Port mismatch in=%" PRIu32 ", state=%" PRIu32 "\n", port, inStateVec.at( vn ).at( vc ).outPort
      );
    switch_alloc_sendto_port = port;
    switch_alloc_sendfrom_vn = vn;
    switch_alloc_sendfrom_vc = vc;
  }

  void resetSwitchSendAllocation() final {
    validateVnVc( switch_alloc_sendfrom_vn, switch_alloc_sendfrom_vc );
    inStateVec.at( switch_alloc_sendfrom_vn ).at( switch_alloc_sendfrom_vc ).inVcState = IN_IDLE;
    switch_alloc_sendto_port = switch_alloc_sendfrom_vn = switch_alloc_sendfrom_vc = UINT32_MAX;
  }

  bool isRecvAllocatedFromSwitch() final { return switch_alloc_rcvto_vn != UINT32_MAX; }

  void recvAllocateFromSwitch( uint32_t sending_port, uint32_t vn, uint32_t vc ) final {
    validateVnVc( vn, vc );
    if( sending_port != outStateVec.at( vn ).at( vc ).inPort )
      output->fatal(
        CALL_INFO, -1, "Port mismatch in=%" PRIu32 ", state=%" PRIu32 "\n", sending_port, outStateVec.at( vn ).at( vc ).inPort
      );
    switch_alloc_rcvfrom_port = sending_port;
    switch_alloc_rcvto_vn     = vn;
    switch_alloc_rcvto_vc     = vc;
  }

  void resetSwitchRecvAllocation() final {
    validateVnVc( switch_alloc_rcvto_vn, switch_alloc_rcvto_vc );
    outStateVec.at( switch_alloc_rcvto_vn ).at( switch_alloc_rcvto_vc ).outVcState = OUT_IDLE;
    switch_alloc_rcvfrom_port = switch_alloc_rcvto_vn = switch_alloc_rcvto_vc = UINT32_MAX;
  }

  uint32_t getDestVc( uint32_t vn, uint32_t vc ) final {
    validateVnVc( vn, vc );
    return inStateVec.at( vn ).at( vc ).outVc;
  }

  uint32_t getSendingPort() final {
    if( switch_alloc_rcvfrom_port == UINT32_MAX )
      return UINT32_MAX;
    validateVnVc( switch_alloc_rcvto_vn, switch_alloc_rcvto_vc );
    if( outStateVec.at( switch_alloc_rcvto_vn ).at( switch_alloc_rcvto_vc ).outBufCredits > 0 )
      return switch_alloc_rcvfrom_port;
    return UINT32_MAX - 1;
  }

  MordredFlit* getInBufFlit() final;
  void         recvOutBufFlit( MordredFlit* flit ) final;

  void resetPerVcDest() final {
    validateVnVc( switch_alloc_sendfrom_vn, switch_alloc_sendfrom_vc );
    inStateVec.at( switch_alloc_sendfrom_vn ).at( switch_alloc_sendfrom_vc ).outPort =
      inStateVec.at( switch_alloc_sendfrom_vn ).at( switch_alloc_sendfrom_vc ).outVn =
        inStateVec.at( switch_alloc_sendfrom_vn ).at( switch_alloc_sendfrom_vc ).outVc = UINT32_MAX;
  }

  void resetPerVcSrc() final {
    validateVnVc( switch_alloc_rcvto_vn, switch_alloc_rcvto_vc );
    outStateVec.at( switch_alloc_rcvto_vn ).at( switch_alloc_rcvto_vc ).inPort =
      outStateVec.at( switch_alloc_rcvto_vn ).at( switch_alloc_rcvto_vc ).inVn =
        outStateVec.at( switch_alloc_rcvto_vn ).at( switch_alloc_rcvto_vc ).inVc = UINT32_MAX;
  }

  uint32_t getConnectedRtrId() const final { return connectedRtrId; }

  /// default constructor (for serialization)
  RtrPortControlPC() : RtrPortControlAPI() {}

  void serialize_order( SST::Core::Serialization::serializer& ser ) override {
    SST_SER( output );
    SST_SER( physChannel );
    SST_SER( topo );
    SST_SER( connectionType );
    SST_SER( rtrId );
    SST_SER( portId );
    SST_SER( connectedRtrId );
    SST_SER( connectedPortId );
    SST_SER( numVns );
    SST_SER( numVcs );
    SST_SER( flitSize );
    SST_SER( flit_vn_rr );
    SST_SER( flit_vc_rr );
    SST_SER( credit_ret_vn_rr );
    SST_SER( credit_ret_vc_rr );
    SST_SER( switch_alloc_sendto_port );
    SST_SER( switch_alloc_sendfrom_vn );
    SST_SER( switch_alloc_sendfrom_vc );
    SST_SER( switch_alloc_rcvfrom_port );
    SST_SER( switch_alloc_rcvto_vn );
    SST_SER( switch_alloc_rcvto_vc );
    SST_SER( inBufSize );
    SST_SER( outBufSize );
    SST_SER( initEvents );
    SST_SER( inStateVec );
    SST_SER( outStateVec );
    SST_SER( rtrSharedObjs );
    SST_SER( statLinkRecvFlitCnt );
    SST_SER( statLinkSentFlitCnt );
    SST_SER( statLinkSentPacketCnt );
    SST_SER( statLinkOutputStalledCnt );
  }

  ImplementSerializable( SST::Mordred::RtrPortControlPC );

private:
  void              allocateBuffers();
  MordredInitEvent* getInitEvent( MordredInitEvent::Commands cmd );
  void              returnCredit();
  void              processIncoming( Event* ev );

  Output*         output{};
  Prydwen::PhysChannelAPI* physChannel{};
  TopologyAPI*    topo{};
  PortConnectionE connectionType{ UNKNOWN };
  uint32_t        rtrId{};
  uint32_t        portId{};
  uint32_t        connectedRtrId{ UINT32_MAX };
  uint32_t        connectedPortId{ UINT32_MAX };
  uint32_t        numVns{ UINT32_MAX };
  uint32_t        numVcs{ UINT32_MAX };
  uint32_t        flitSize{};
  uint32_t        flit_vn_rr{};
  uint32_t        flit_vc_rr{};
  uint32_t        credit_ret_vn_rr{};
  uint32_t        credit_ret_vc_rr{};

  uint32_t switch_alloc_sendto_port{ UINT32_MAX };
  uint32_t switch_alloc_sendfrom_vn{ UINT32_MAX };
  uint32_t switch_alloc_sendfrom_vc{ UINT32_MAX };
  uint32_t switch_alloc_rcvfrom_port{ UINT32_MAX };
  uint32_t switch_alloc_rcvto_vn{ UINT32_MAX };
  uint32_t switch_alloc_rcvto_vc{ UINT32_MAX };

  uint32_t inBufSize{};
  uint32_t outBufSize{};

  std::queue<Event*> initEvents;

  std::vector<std::vector<perVcInState>>  inStateVec;
  std::vector<std::vector<perVcOutState>> outStateVec;

  RtrOwnedSharedObjs* rtrSharedObjs{};

  std::vector<std::vector<Statistic<uint64_t>*>> statLinkRecvFlitCnt;
  std::vector<std::vector<Statistic<uint64_t>*>> statLinkSentFlitCnt;
  std::vector<std::vector<Statistic<uint64_t>*>> statLinkSentPacketCnt;
  Statistic<uint64_t>*                           statLinkOutputStalledCnt{};
};

}  // namespace SST::Mordred
#endif  // MORDRED_RTRPORTCONTROLPC_H
