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
#include <deque>

namespace SST::Mordred {

/**
 * RtrPortControlPC — router port controller backed by a PhysChannelAPI subcomponent.
 *
 * The inner PhysChannelAPI can be any matching implementation:
 *   prydwen.genericPhysChannel — for tests
 *   prydwen.uciePhysChannel    — for production UCIe physical links
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
      "SST::Prydwen::PhysChannelAPI" }
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

  ~RtrPortControlPC() override = default;

  // Inner-channel receive notification callback
  bool onReceive( int sn_vn );

  // Override ClockTick to drip one pending flit per tick. UCIe's link-timing model
  // can complete several independent single-flit sends within the same SST cycle
  // (this is not FlitFactory reassembly — transportSendFlit() always uses
  // packet_size_bits=0, so the factory is never invoked); without this queue those
  // flits would all be processed in the tick they arrive, saturating inBuf.
  // TODO (tdysart): Look at the function and make sense of this comment
  void ClockTick( Cycle_t cycle ) override;


  RtrPortControlPC() : RtrPortControlBase() {}

  void serialize_order( SST::Core::Serialization::serializer& ser ) override {
    RtrPortControlBase::serialize_order( ser );
    SST_SER( physChannel );
    SST_SER( pendingFlits_ );
  }

  ImplementSerializable( SST::Mordred::RtrPortControlPC );

protected:
  void transportSendFlit( MordredFlit* flit, uint32_t vn ) override {
    // Send every Mordred flit (HEAD, BODY, TAIL) individually as a single-flit
    // UCIe message (packet_size_bits=0 → m_count=0 → direct delivery, no
    // FlitFactory). This matches the original per-flit send behaviour that was
    // present at commit 713ea49, where each flit traverses UCIe independently
    // as its own atomic message. UCIe's transfer timing can still land more than
    // one such flit in the same tick; the pendingFlits_/ClockTick drip queue below
    // is what re-serializes those to one-per-tick, native-link-equivalent timing.
    // TODO (tdysart): Look at the function and make sense of this comment
    Prydwen::PhysChannelFlitDescriptor desc;
    desc.flit             = flit;
    desc.req              = nullptr;   // not used when packet_size_bits=0
    desc.packet_id        = 0;
    desc.packet_size_bits = 0;         // → m_count=0 → direct delivery
    desc.is_first         = true;      // each flit is its own self-contained event
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

  bool transportSpaceToSend( uint32_t vn ) override {
    return physChannel->spaceToSend( static_cast<int>( vn ), static_cast<int>( flitSize ) );
  }

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
          output->fatal( CALL_INFO, -1, "RtrPortControlPC FlitFactory: head_event is not a MordredFlit\n" );
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

private:
  Prydwen::PhysChannelAPI* physChannel{};

  // Pending flit queue: UCIe's link-timing model can deliver more than one
  // independent single-flit message within the same SST cycle (each still sent
  // via transportSendFlit() with packet_size_bits=0 — this is not FlitFactory
  // reassembly), but the router's input state machine expects flits to arrive
  // one per clock tick (matching native link timing). We queue incoming events
  // here and inject one per ClockTick to match native link behaviour and avoid
  // inBuf saturation leading to deadlock.
  // Credit events bypass this queue and are processed immediately.
  std::deque<SST::Event*> pendingFlits_;
};

}  // namespace SST::Mordred

#endif  // MORDRED_RTRPORTCONTROLPC_H
