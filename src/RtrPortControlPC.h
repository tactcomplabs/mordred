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

#include "RtrPortControlBase.h"
#include "PhysChannelAPI.h"

namespace SST::Mordred {

/**
 * RtrPortControlPC — router port controller backed by a PhysChannelAPI subcomponent.
 *
 * The inner PhysChannelAPI can be any matching implementation:
 *   mordred.genericPhysChannel — for tests
 *   ucie.ucieInterfaceSN      — for production UCIe physical links
 */
class RtrPortControlPC : public RtrPortControlBase {
public:
  SST_ELI_REGISTER_SUBCOMPONENT(
    RtrPortControlPC,
    "mordred",
    "rtrPortControlPC",
    SST_ELI_ELEMENT_VERSION( 0, 1, 0 ),
    "SimpleNetwork-backed port control for the Mordred router",
    SST::Mordred::RtrPortControlAPI
  )

  SST_ELI_DOCUMENT_PARAMS()

  SST_ELI_DOCUMENT_PORTS()

  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
    { "port_iface", "PhysChannelAPI subcomponent that manages the physical port link",
      "SST::Mordred::PhysChannelAPI" }
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

  // Inner-channel receive notification callback
  bool onReceive( int sn_vn );

  RtrPortControlPC() : RtrPortControlBase() {}

  void serialize_order( SST::Core::Serialization::serializer& ser ) override {
    RtrPortControlBase::serialize_order( ser );
    SST_SER( physChannel );
  }

  ImplementSerializable( SST::Mordred::RtrPortControlPC );

protected:
  void transportSendFlit( MordredFlit* flit, uint32_t vn ) override {
    physChannel->send( flit, static_cast<int>( vn ) );
  }
  void transportSendCredit( MordredCreditEvent* credit, uint32_t vn ) override {
    physChannel->send( credit, static_cast<int>( vn ) );
  }
  void        transportSendUntimedData( SST::Event* ev ) override { physChannel->sendUntimedData( ev ); }
  SST::Event* transportRecvUntimedData() override { return physChannel->recvUntimedData(); }

  bool transportSpaceToSend( uint32_t vn ) override {
    return physChannel->spaceToSend( static_cast<int>( vn ), static_cast<int>( flitSize ) );
  }

  void transportInit( uint32_t phase ) override { physChannel->init( phase ); }
  void transportSetup() override { physChannel->setup(); }
  void transportComplete( uint32_t phase ) override { physChannel->complete( phase ); }

private:
  Prydwen::PhysChannelAPI* physChannel{};
};

}  // namespace SST::Mordred

#endif  // MORDRED_RTRPORTCONTROLPC_H
