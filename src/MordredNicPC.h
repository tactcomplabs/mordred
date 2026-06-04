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
    // Same per-flit individual delivery as RtrPortControlPC — see that file.
    Prydwen::PhysChannelFlitDescriptor desc;
    desc.flit             = flit;
    desc.req              = nullptr;
    desc.packet_id        = 0;
    desc.packet_size_bits = 0;
    desc.is_first         = true;
    desc.is_last          = true;
    physChannel->send( desc, static_cast<int>( vn ) );
  }
  void transportSendCredit( MordredCreditEvent* credit, uint32_t vn ) override {
    Prydwen::PhysChannelFlitDescriptor desc;
    desc.flit             = credit;
    desc.req              = nullptr;
    desc.packet_id        = 0;
    desc.packet_size_bits = 0;
    desc.is_first         = true;
    desc.is_last          = true;
    physChannel->send( desc, static_cast<int>( vn ) );
  }
  void        transportSendUntimedData( SST::Event* ev ) override { physChannel->sendUntimedData( ev ); }
  SST::Event* transportRecvUntimedData() override { return physChannel->recvUntimedData(); }

  void transportInit( uint32_t phase ) override { physChannel->init( phase ); }
  void transportSetup() override {
    // Inform the physical channel of Mordred's logical flit width so it can
    // compute m_count = ceil(packet_size_bits / network_flit_size) on send.
    if( flitSize > 0 )
      physChannel->setNetworkFlitSize( flitSize );

    // Register the factory that synthesizes M Mordred flits from the deserialized
    // HEAD flit on the receive side.
    physChannel->setFlitFactory(
      [this]( SST::Event* head_event, uint32_t m_count, int /*ext_vn*/ ) -> std::vector<SST::Event*> {
        auto* hf = dynamic_cast<MordredFlit*>( head_event );
        if( !hf )
          output->fatal( CALL_INFO, -1, "MordredNicPC FlitFactory: head_event is not a MordredFlit\n" );
        auto*                    req = hf->req;
        std::vector<SST::Event*> result;
        result.reserve( m_count );
        result.push_back( hf );  // HEAD — already constructed correctly
        for( uint32_t i = 1; i < m_count - 1; i++ ) {
          auto* body              = new MordredFlit( req, MordredFlit::BODY, hf->packet_id, i );
          body->cur_vc            = hf->cur_vc;            // preserve VC assignment
          body->pkt_created_cycle = hf->pkt_created_cycle;
          body->head_inject_cycle = hf->head_inject_cycle;
          result.push_back( body );
        }
        auto* tail              = new MordredFlit( req, MordredFlit::TAIL, hf->packet_id, m_count - 1 );
        tail->cur_vc            = hf->cur_vc;              // preserve VC assignment
        tail->pkt_created_cycle = hf->pkt_created_cycle;
        tail->head_inject_cycle = hf->head_inject_cycle;
        result.push_back( tail );
        return result;
      }
    );

    physChannel->setup();
  }
  void transportComplete( uint32_t phase ) override { physChannel->complete( phase ); }
  void transportFinish() override { physChannel->finish(); }

private:
  Prydwen::PhysChannelAPI* physChannel{};
};

}  // namespace SST::Mordred

#endif  // MORDRED_MORDREDNICPC_H
