//
// GenericPhysChannel.h
//
// Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef MORDRED_GENERICPHYSCHANNEL_H
#define MORDRED_GENERICPHYSCHANNEL_H

// Standard headers
#include <queue>
#include <string>
#include <vector>

// Local SST config
#include "sst_config.h"

#include "PhysChannelAPI.h"

namespace SST::Mordred {

/**
 * Minimal SST::Event wrapper that carries a raw payload and VN tag across a
 * raw SST::Link. Used exclusively by GenericPhysChannel.
 *
 * Replacing the old RequestWrapperEvent: we no longer need to carry the full
 * SimpleNetwork::Request — just the payload event and the VN index.
 */
class PhysChannelLinkEvent : public SST::Event {
public:
  SST::Event* payload{nullptr};
  int         vn{0};

  PhysChannelLinkEvent() = default;
  PhysChannelLinkEvent( SST::Event* p, int v ) : payload( p ), vn( v ) {}

  void serialize_order( SST::Core::Serialization::serializer& ser ) override {
    Event::serialize_order( ser );
    SST_SER( payload );
    SST_SER( vn );
  }

  ImplementSerializable( SST::Mordred::PhysChannelLinkEvent );
};

/**
 * Format-agnostic PhysChannelAPI implementation that forwards any SST::Event*
 * over a raw SST::Link by wrapping it in a PhysChannelLinkEvent (to preserve
 * the VN tag across the wire).
 *
 * Both ends of the link must use a GenericPhysChannel (or another component
 * that understands the PhysChannelLinkEvent wire format).
 *
 * Port name: the "port_name" parameter names the SST::Link that
 * configureLink() will use.
 */
class GenericPhysChannel : public PhysChannelAPI {
public:
  SST_ELI_REGISTER_SUBCOMPONENT(
    GenericPhysChannel,
    "mordred",
    "genericPhysChannel",
    SST_ELI_ELEMENT_VERSION( 0, 2, 0 ),
    "Generic PhysChannelAPI adapter that forwards events over a raw SST::Link via PhysChannelLinkEvent",
    SST::Mordred::PhysChannelAPI
  )

  SST_ELI_DOCUMENT_PARAMS(
    {"port_name", "Name of the SST::Link to configure (e.g. 'port0', 'port')", "port"},
    {"verbose",   "Output verbosity", "0"}
  )

  // Uses SHARE_PORTS — parent provides the physical link namespace
  SST_ELI_DOCUMENT_PORTS()

  SST_ELI_DOCUMENT_STATISTICS()

  GenericPhysChannel( ComponentId_t id, Params& params, int num_vns );
  ~GenericPhysChannel() override = default;

  // --- PhysChannelAPI interface ---

  bool        send( SST::Event* payload, int vn ) override;
  SST::Event* recv( int vn ) override;

  void        sendUntimedData( SST::Event* payload ) override;
  SST::Event* recvUntimedData() override;

  void init(     unsigned int phase ) override;
  void setup()                        override;
  void complete( unsigned int phase ) override;

  bool spaceToSend( int /*vn*/, int /*num_bits*/ ) override { return true; }

  bool requestToReceive( int vn ) override {
    if ( vn < 0 || vn >= static_cast<int>( recvQueues.size() ) ) return false;
    return !recvQueues.at( static_cast<size_t>( vn ) ).empty();
  }

  void setNotifyOnReceive( HandlerBase* handler ) override { recvNotifyHandler = handler; }
  void setNotifyOnSend(    HandlerBase* /*handler*/ ) override { /* not used */ }

  bool isNetworkInitialized() const override { return initialized; }
  const UnitAlgebra& getLinkBW() const override { return linkBW; }

  // Incoming link event handler (registered on the SST::Link)
  void handleIncoming( SST::Event* ev );

  // --- Default constructor for serialization ---
  GenericPhysChannel() : PhysChannelAPI() {}

  void serialize_order( SST::Core::Serialization::serializer& ser ) override {
    PhysChannelAPI::serialize_order( ser );
    SST_SER( output );
    SST_SER( link );
    SST_SER( portName );
    SST_SER( numVns );
    SST_SER( initialized );
    SST_SER( linkBW );
    SST_SER( recvNotifyHandler );
    SST_SER( recvQueues );
  }

  ImplementSerializable( SST::Mordred::GenericPhysChannel );

private:
  Output*      output{};
  Link*        link{};
  std::string  portName;
  int          numVns{1};
  bool         initialized{false};
  UnitAlgebra  linkBW;
  HandlerBase* recvNotifyHandler{nullptr};

  // Per-VN receive queues; outer index == VN
  std::vector<std::queue<SST::Event*>> recvQueues;
};

} // namespace SST::Mordred
#endif // MORDRED_GENERICPHYSCHANNEL_H
