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

/**
 * MordredNicPC is a version of MordredNIC where the raw SST::Link to the
 * router is replaced by an inner SST::Interfaces::SimpleNetwork subcomponent
 * (slot "port_iface").  From the perspective of the outer host component
 * (e.g. merlin.test_nic), this class is identical to MordredNIC — it still
 * IS a SimpleNetwork.
 *
 * The inner PhysChannelAPI can be any matching implementation:
 *   mordred.genericPhysChannel — for tests (generic raw-link wrapper)
 *   ucie.ucieInterfaceSN      — for production UCIe physical links
 *
 * Wire format: the inner PhysChannelAPI determines the on-wire format. With
 * genericPhysChannel, the link carries PhysChannelLinkEvent objects.
 * Both ends must use the PhysChannelAPI-backed path.
 *
 * Mordred's own credit protocol is unchanged.  The inner channel's flow
 * control (e.g. UCIe lane credits via spaceToSend()) and Mordred's
 * MordredCreditEvent are complementary: the inner channel protects the local
 * TX buffer; Mordred credits protect the remote router's input buffer.
 */

// Standard headers
#include <cinttypes>
#include <queue>
#include <vector>

// Local SST config
#include "sst_config.h"

// Local headers
#include "MordredEvents.h"
#include "PhysChannelAPI.h"

#include <sst/core/interfaces/simpleNetwork.h>

namespace SST::Mordred {

class MordredNicPC : public Interfaces::SimpleNetwork {

public:
  SST_ELI_REGISTER_SUBCOMPONENT(
    MordredNicPC,
    "mordred",
    "mordredNicPC",
    SST_ELI_ELEMENT_VERSION( 0, 1, 0 ),
    "SimpleNetwork-backed Mordred NIC endpoint; uses inner SimpleNetwork for physical link",
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
    "Port that connects to a Mordred router (via inner SimpleNetwork adapter).",
    { "untimedMordredEvent", "basicMordredEvent" }
  } )

  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
    { "port_iface", "PhysChannelAPI subcomponent that manages the physical link to the router", "SST::Mordred::PhysChannelAPI" }
  )

  SST_ELI_DOCUMENT_STATISTICS(
    { "packets_recv", "Number of packets received", "unitless", 3 },
    { "average_noc_latency", "Average latency (in clocks) of each packet", "unitless", 3 },
    { "average_packet_size", "Average packet size in number of flits", "unitless", 3 }
  )

  MordredNicPC( ComponentId_t cid, Params& params, int vns );
  ~MordredNicPC() override = default;

  // SST lifecycle
  void init( uint32_t phase ) override;
  void setup() override;
  void complete( uint32_t phase ) override;
  void finish() override;

  // SimpleNetwork outer interface (used by merlin.test_nic etc.)
  void     sendUntimedData( Request* req ) override;
  Request* recvUntimedData() override;

  bool     send( Request* req, int vn ) override;
  Request* recv( int vn ) override;

  bool spaceToSend( int vn, int num_bits ) override;
  bool requestToReceive( int vn ) override;

  void setNotifyOnReceive( HandlerBase* functor ) override { receiveFunctor = functor; }

  void setNotifyOnSend( HandlerBase* functor ) override { sendFunctor = functor; }

  bool isNetworkInitialized() const override { return initialized; }

  nid_t getEndpointID() const override { return netID; }

  const UnitAlgebra& getLinkBW() const override { return bw; }

  // Clock handler
  bool clockTick( Cycle_t cycle );

  // Inner-SN receive notification callback
  bool onReceive( int sn_vn );

  // Default constructor for serialization
  MordredNicPC() : Interfaces::SimpleNetwork() {}

  void serialize_order( SST::Core::Serialization::serializer& ser ) override {
    SST_SER( output );
    SST_SER( physChannel );
    SST_SER( netID );
    SST_SER( rtrId );
    SST_SER( rtrPort );
    SST_SER( initialized );
    SST_SER( numVns );
    SST_SER( numVcs );
    SST_SER( flitSize );
    SST_SER( packetId );
    SST_SER( headInjectCycle );
    SST_SER( bw );
    SST_SER( inbufSize );
    SST_SER( outbufSize );
    SST_SER( initEvents );
    SST_SER( inBuf );
    SST_SER( outBuf );
    SST_SER( rtrCredits );
    SST_SER( outbufCredits );
    SST_SER( totalNocLatency );
    SST_SER( totalPackets );
    SST_SER( totalNumFlits );
    SST_SER( statPacketsRecv );
    SST_SER( statAvgNocLatency );
    SST_SER( statAvgFlitsPerPacket );
  }

  ImplementSerializable( SST::Mordred::MordredNicPC );

private:
  void              resizeVectors();
  MordredInitEvent* getInitEvent( MordredInitEvent::Commands cmd );
  int32_t           calcNumFlits( uint32_t num_bits );

  // Dispatch incoming event from inner SN
  void processIncoming( SST::Event* ev );

  Output*         output{};
  PhysChannelAPI* physChannel{};

  nid_t    netID{ -1 };
  uint32_t rtrId{ UINT32_MAX };
  uint32_t rtrPort{ UINT32_MAX };
  bool     initialized{ false };
  uint32_t numVns{ 0 };
  uint32_t numVcs{ UINT32_MAX };
  uint32_t flitSize{};
  uint64_t packetId{};
  uint64_t headInjectCycle{ UINT64_MAX };

  UnitAlgebra bw;

  HandlerBase* sendFunctor{ nullptr };
  HandlerBase* receiveFunctor{ nullptr };

  // in bits
  UnitAlgebra inbufSize;
  UnitAlgebra outbufSize;

  std::queue<MordredInitEvent*> initEvents;

  // Per-VN packet and flit buffers
  std::vector<std::queue<Request*>>     inBuf;   // from router, reassembled packets
  std::vector<std::queue<MordredFlit*>> outBuf;  // to router, pending flits

  // Credit counters (1 credit == 1 flit)
  std::vector<int32_t> rtrCredits;       // credits from router for outgoing flits
  std::vector<int32_t> outbufCredits;    // space in outBuf (from endpoint)
  std::vector<int32_t> inReturnCredits;  // credits to return to router

  // Statistics
  uint64_t             totalNocLatency{ 0 };
  uint64_t             totalPackets{ 0 };
  uint64_t             totalNumFlits{ 0 };
  Statistic<uint64_t>* statPacketsRecv{};
  Statistic<double>*   statAvgNocLatency{};
  Statistic<double>*   statAvgFlitsPerPacket{};
};

}  // namespace SST::Mordred
#endif  // MORDRED_MORDREDNICPC_H
