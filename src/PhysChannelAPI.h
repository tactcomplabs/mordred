//
// PhysChannelAPI.h
//
// Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef MORDRED_PHYSCHANNEL_API_H
#define MORDRED_PHYSCHANNEL_API_H

#include "sst_config.h"

namespace SST::Mordred {

/**
 * Abstract base class for Mordred physical channel subcomponents.
 *
 * A physical channel moves SST::Event* objects between two endpoints over a
 * raw SST::Link. It is the pluggable transport layer beneath MordredNicSN and
 * RtrPortControlSN. Multiple virtual networks (VNs) are supported for
 * ordering and flow-control separation.
 *
 * VN count is passed as a constructor argument so implementations can size
 * internal queues at construction time.
 *
 * Callers pass their native event types (MordredFlit, MordredCreditEvent, …)
 * directly. No SimpleNetwork::Request wrapper is required or produced.
 */
class PhysChannelAPI : public SST::SubComponent {
public:
  SST_ELI_REGISTER_SUBCOMPONENT_API(SST::Mordred::PhysChannelAPI, int)

  using HandlerBase = SSTHandlerBase<bool, int>;

  template <typename classT, auto funcT, typename dataT = void>
  using Handler2 = SSTHandler2<bool, int, classT, dataT, funcT>;

  PhysChannelAPI( ComponentId_t id, Params& /*params*/, int /*num_vns*/ )
    : SubComponent( id ) {}

  ~PhysChannelAPI() override = default;

  // --- Timed send / receive ---

  virtual bool        send( SST::Event* payload, int vn ) = 0;
  virtual SST::Event* recv( int vn ) = 0;

  // --- Untimed phases (init / complete) ---

  virtual void        sendUntimedData( SST::Event* payload ) = 0;
  virtual SST::Event* recvUntimedData() = 0;

  // --- Flow control ---

  // Default returns true; implementations backed by hardware (e.g. UCIe) may
  // override to reflect actual TX buffer availability.
  virtual bool spaceToSend( int /*vn*/, int /*num_bits*/ ) { return true; }
  virtual bool requestToReceive( int vn ) = 0;

  // --- Receive / send notification ---

  virtual void setNotifyOnReceive( HandlerBase* handler ) = 0;
  virtual void setNotifyOnSend(    HandlerBase* /*handler*/ ) {} // optional; no-op default

  // --- Status ---

  virtual bool               isNetworkInitialized() const = 0;
  virtual const UnitAlgebra& getLinkBW() const = 0;

  // --- Serialization ---

  PhysChannelAPI() : SubComponent() {}

  void serialize_order( SST::Core::Serialization::serializer& ser ) override {
    SubComponent::serialize_order( ser );
  }
};

} // namespace SST::Mordred
#endif // MORDRED_PHYSCHANNEL_API_H
