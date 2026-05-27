//
// MordredNicPC.h
//
// Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef MORDRED_MORDREDNICPC_H
#define MORDRED_MORDREDNICPC_H

#include "MordredNicBase.h"
#include "PhysChannelAPI.h"

namespace SST::Mordred {

/**
 * MordredNicPC — SST SimpleNetwork NIC backed by a PhysChannelAPI subcomponent.
 *
 * The inner PhysChannelAPI can be any matching implementation:
 *   mordred.genericPhysChannel — for tests (generic raw-link wrapper)
 *   ucie.ucieInterfaceSN      — for production UCIe physical links
 *
 * Mordred's own credit protocol is unchanged.  The inner channel's flow
 * control and Mordred's MordredCreditEvent are complementary: the inner
 * channel protects the local TX buffer; Mordred credits protect the remote
 * router's input buffer.
 */
class MordredNicPC : public MordredNicBase {

public:
  SST_ELI_REGISTER_SUBCOMPONENT(
    MordredNicPC,
    "mordred",
    "mordredNicPC",
    SST_ELI_ELEMENT_VERSION( 0, 1, 0 ),
    "SimpleNetwork-backed Mordred NIC endpoint; uses inner PhysChannelAPI for physical link",
    SST::Interfaces::SimpleNetwork
  )

  SST_ELI_DOCUMENT_PARAMS(
    { "verbose", "Sets the output verbosity", "5" },
    { "clock", "Clock frequency of this interface", "1GHz" },
    { "input_buf_size", "Size of input buffers specified in b or B (can include SI prefix).", "1kiB" },
    { "output_buf_size", "Size of output buffers specified in b or B (can include SI prefix).", "1kiB" }
  )

  SST_ELI_DOCUMENT_PORTS( {
    "port",
    "Port that connects to a Mordred router (via inner PhysChannelAPI adapter).",
    { "untimedMordredEvent", "basicMordredEvent" }
  } )

  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
    { "port_iface", "PhysChannelAPI subcomponent that manages the physical link to the router",
      "SST::Mordred::PhysChannelAPI" }
  )

  SST_ELI_DOCUMENT_STATISTICS(
    { "packets_recv", "Number of packets received", "unitless", 3 },
    { "average_noc_latency", "Average latency (in clocks) of each packet", "unitless", 3 },
    { "average_packet_size", "Average packet size in number of flits", "unitless", 3 }
  )

  MordredNicPC( ComponentId_t cid, Params& params, int vns );
  ~MordredNicPC() override = default;

  // Inner-channel receive notification callback
  bool onReceive( int sn_vn );

  MordredNicPC() : MordredNicBase() {}

  void serialize_order( SST::Core::Serialization::serializer& ser ) override {
    MordredNicBase::serialize_order( ser );
    SST_SER( physChannel );
  }

  ImplementSerializable( SST::Mordred::MordredNicPC );

protected:
  void transportSendFlit( MordredFlit* flit, uint32_t vn ) override {
    physChannel->send( flit, static_cast<int>( vn ) );
  }
  void transportSendCredit( MordredCreditEvent* credit, uint32_t vn ) override {
    physChannel->send( credit, static_cast<int>( vn ) );
  }
  void        transportSendUntimedData( SST::Event* ev ) override { physChannel->sendUntimedData( ev ); }
  SST::Event* transportRecvUntimedData() override { return physChannel->recvUntimedData(); }

  void transportInit( uint32_t phase ) override { physChannel->init( phase ); }
  void transportSetup() override { physChannel->setup(); }
  void transportComplete( uint32_t phase ) override { physChannel->complete( phase ); }

private:
  Prydwen::PhysChannelAPI* physChannel{};
};

}  // namespace SST::Mordred

#endif  // MORDRED_MORDREDNICPC_H
