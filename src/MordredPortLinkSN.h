//
// MordredPortLinkSN.h
//
// Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef MORDRED_MORDREDPORTLINKSN_H
#define MORDRED_MORDREDPORTLINKSN_H

// Standard headers
#include <queue>
#include <string>
#include <vector>

// Local SST config
#include "sst_config.h"
#include "MordredEvents.h"

#include <sst/core/interfaces/simpleNetwork.h>

namespace SST::Mordred {

/**
 * Thin SST::Interfaces::SimpleNetwork adapter for use with RtrPortControlSN.
 *
 * Preserves the existing Mordred wire format: the physical SST::Link carries
 * baseMordredEvent objects (MordredFlit and MordredCreditEvent) directly,
 * exactly as in the original RtrPortControl implementation.  This makes
 * MordredPortLinkSN compatible with both mordredNIC endpoints and other
 * MordredPortLinkSN-backed router ports.
 *
 * Outgoing: Request payload (baseMordredEvent*) is extracted and sent on link.
 * Incoming: baseMordredEvent from link is wrapped in a Request and queued per-VN.
 *
 * VN determination for incoming events:
 *   MordredFlit       → flit->vn
 *   MordredCreditEvent → credit->vn
 *
 * Flow control: spaceToSend() always returns true; Mordred's own credit protocol
 * is the only backpressure mechanism needed here.
 *
 * Port name: the "port_name" parameter (required) names the physical link that
 * configureLink() will use.  Set this to "portN" matching the router port.
 */
class MordredPortLinkSN : public Interfaces::SimpleNetwork {
public:
  SST_ELI_REGISTER_SUBCOMPONENT(
    MordredPortLinkSN,
    "mordred",
    "mordredPortLinkSN",
    SST_ELI_ELEMENT_VERSION( 0, 1, 0 ),
    "Thin SimpleNetwork adapter for RtrPortControlSN; preserves Mordred wire format",
    SST::Interfaces::SimpleNetwork
  )

  SST_ELI_DOCUMENT_PARAMS(
    {"port_name",  "Name of the router port/link to configure (e.g. 'port0')", "port"},
    {"verbose",    "Output verbosity",                                           "0"}
  )

  // Uses SHARE_PORTS — accesses the parent router's port namespace directly
  SST_ELI_DOCUMENT_PORTS()

  SST_ELI_DOCUMENT_STATISTICS()

  MordredPortLinkSN( ComponentId_t id, Params& params, int num_vns );

  ~MordredPortLinkSN() override = default;

  // --- SimpleNetwork interface ---

  void    sendUntimedData(Request* req) override;
  Request* recvUntimedData()            override;

  bool     send(Request* req, int vn) override;
  Request* recv(int vn)               override;

  void     init(unsigned int phase)     override;
  void     setup()                      override;
  void     complete(unsigned int phase) override;

  bool spaceToSend(int /*vn*/, int /*num_bits*/) override { return true; }

  bool requestToReceive(int vn) override {
    if ( vn < 0 || vn >= static_cast<int>(recvQueues.size()) ) return false;
    return !recvQueues.at(static_cast<size_t>(vn)).empty();
  }

  void setNotifyOnReceive(HandlerBase* handler) override { recvNotifyHandler = handler; }
  void setNotifyOnSend(HandlerBase* /*handler*/) override { /* not used */ }

  bool isNetworkInitialized() const override { return initialized; }
  nid_t getEndpointID()       const override { return 0; }
  const UnitAlgebra& getLinkBW() const override { return linkBW; }

  // Incoming link event handler
  void handleIncoming(SST::Event* ev);

  // --- Default constructor for serialization ---
  MordredPortLinkSN() : Interfaces::SimpleNetwork() {}

  void serialize_order(SST::Core::Serialization::serializer& ser) override {
    SimpleNetwork::serialize_order(ser);
    SST_SER(output);
    SST_SER(link);
    SST_SER(portName);
    SST_SER(numVns);
    SST_SER(initialized);
    SST_SER(linkBW);
    SST_SER(recvNotifyHandler);
    SST_SER(recvQueues);
  }

  ImplementSerializable(SST::Mordred::MordredPortLinkSN);

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
#endif // MORDRED_MORDREDPORTLINKSN_H
