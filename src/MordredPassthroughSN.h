//
// MordredPassthroughSN.h
//
// Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef MORDRED_MORDREDPASSTHROUGHSN_H
#define MORDRED_MORDREDPASSTHROUGHSN_H

// Standard headers
#include <queue>
#include <string>
#include <vector>

// Local SST config
#include "sst_config.h"

#include <sst/core/interfaces/simpleNetwork.h>

namespace SST::Mordred {

/**
 * Generic SST::Event wrapper that carries a SimpleNetwork::Request across a
 * raw SST::Link.  Used exclusively by MordredPassthroughSN.
 *
 * The entire Request (including its vn field and payload) is preserved, so
 * the receive side can determine VN without inspecting the payload type.
 * SimpleNetwork::Request implements serialize_order, so checkpoint/restart
 * works correctly.
 */
class RequestWrapperEvent : public SST::Event {
public:
  Interfaces::SimpleNetwork::Request* req{nullptr};

  RequestWrapperEvent() = default;
  explicit RequestWrapperEvent( Interfaces::SimpleNetwork::Request* r ) : req( r ) {}

  void serialize_order( SST::Core::Serialization::serializer& ser ) override {
    Event::serialize_order( ser );
    SST_SER( req );
  }

  ImplementSerializable( SST::Mordred::RequestWrapperEvent );
};

/**
 * Truly generic SimpleNetwork adapter that wraps the entire SimpleNetwork::Request
 * in a RequestWrapperEvent for transmission on a raw SST::Link.
 *
 * No knowledge of Mordred-specific event types: VN is read directly from
 * req->vn (which callers such as RtrPortControlSN always set before send()).
 *
 * This component is the counterpart to MordredPortLinkSN.  The difference:
 *   MordredPortLinkSN   — extracts the baseMordredEvent payload; link carries
 *                          raw Mordred events (wire-format compatible with
 *                          MordredNIC / RtrPortControl).
 *   MordredPassthroughSN — wraps the whole Request; link carries
 *                          RequestWrapperEvent objects (requires the other end
 *                          to also be a MordredPassthroughSN or equivalent).
 *
 * Use MordredPassthroughSN when both ends are SN-backed components
 * (e.g. RtrPortControlSN ↔ MordredNicSN).  Use MordredPortLinkSN for the
 * transitional case where one end is still the legacy direct-link MordredNIC.
 *
 * Port name: the "port_name" parameter names the physical SST::Link that
 * configureLink() will use.
 */
class MordredPassthroughSN : public Interfaces::SimpleNetwork {
public:
  SST_ELI_REGISTER_SUBCOMPONENT(
    MordredPassthroughSN,
    "mordred",
    "mordredPassthroughSN",
    SST_ELI_ELEMENT_VERSION( 0, 1, 0 ),
    "Generic SimpleNetwork adapter that forwards Requests over a raw SST::Link via RequestWrapperEvent",
    SST::Interfaces::SimpleNetwork
  )

  SST_ELI_DOCUMENT_PARAMS(
    {"port_name", "Name of the SST::Link to configure (e.g. 'port0', 'port')", "port"},
    {"verbose",   "Output verbosity", "0"}
  )

  // Uses SHARE_PORTS — parent provides the physical link namespace
  SST_ELI_DOCUMENT_PORTS()

  SST_ELI_DOCUMENT_STATISTICS()

  MordredPassthroughSN( ComponentId_t id, Params& params, int num_vns );
  ~MordredPassthroughSN() override = default;

  // --- SimpleNetwork interface ---

  void     sendUntimedData( Request* req ) override;
  Request* recvUntimedData()               override;

  bool     send( Request* req, int vn )    override;
  Request* recv( int vn )                  override;

  void     init( unsigned int phase )      override;
  void     setup()                         override;
  void     complete( unsigned int phase )  override;

  bool spaceToSend( int /*vn*/, int /*num_bits*/ ) override { return true; }

  bool requestToReceive( int vn ) override {
    if ( vn < 0 || vn >= static_cast<int>( recvQueues.size() ) ) return false;
    return !recvQueues.at( static_cast<size_t>( vn ) ).empty();
  }

  void setNotifyOnReceive( HandlerBase* handler ) override { recvNotifyHandler = handler; }
  void setNotifyOnSend( HandlerBase* /*handler*/ ) override { /* not used */ }

  bool isNetworkInitialized() const override { return initialized; }
  nid_t getEndpointID()       const override { return 0; }
  const UnitAlgebra& getLinkBW() const override { return linkBW; }

  // Incoming link event handler (registered on the SST::Link)
  void handleIncoming( SST::Event* ev );

  // --- Default constructor for serialization ---
  MordredPassthroughSN() : Interfaces::SimpleNetwork() {}

  void serialize_order( SST::Core::Serialization::serializer& ser ) override {
    SimpleNetwork::serialize_order( ser );
    SST_SER( output );
    SST_SER( link );
    SST_SER( portName );
    SST_SER( numVns );
    SST_SER( initialized );
    SST_SER( linkBW );
    SST_SER( recvNotifyHandler );
    SST_SER( recvQueues );
  }

  ImplementSerializable( SST::Mordred::MordredPassthroughSN );

private:
  Output*       output{};
  Link*         link{};
  std::string   portName;
  int           numVns{1};
  bool          initialized{false};
  UnitAlgebra   linkBW;
  HandlerBase*  recvNotifyHandler{nullptr};

  // Per-VN receive queues; outer index == VN
  std::vector<std::queue<Request*>> recvQueues;
};

} // namespace SST::Mordred
#endif // MORDRED_MORDREDPASSTHROUGHSN_H
