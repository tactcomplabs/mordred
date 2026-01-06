//
// MordredNIC.h
//
// Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef MORDREDNIC_H
#define MORDREDNIC_H


/**
 * The NIC is designed to get most of its configuration information from the SimpleRtr
 * rather than as outside parameters.  If we modify this behavior, then we'll need to
 * change the initialization procedure to match.
 *
 * The endpoint is expected to poll the NIC to get messages out of it. Haven't written/tested
 * the code yet to do it via event handlers
 *
 * Uses the SST SimpleNetwork interface
 */


// Standard headers
#include <cinttypes>
#include <vector>
#include <queue>

// Local SST header
#include "sst_config.h"

// Local headers
#include "MordredEvents.h"

namespace SST::Mordred {

class MordredNIC : public Interfaces::SimpleNetwork {

public:
  SST_ELI_REGISTER_SUBCOMPONENT(
    MordredNIC,
    "mordred",
    "mordredNIC",
    SST_ELI_ELEMENT_VERSION( 0, 1, 0 ),
    "Interface for connecting to the Mordred NOC",
    SST::Interfaces::SimpleNetwork
  )

  SST_ELI_DOCUMENT_PARAMS(
    { "verbose",         "Sets the output verbosity", "5" },
    { "clock",            "Clock frequency of this interface", "1GHz"},
    { "input_buf_size",  "Size of input buffers specified in b or B (can include SI prefix).", "1kiB"},
    { "output_buf_size", "Size of output buffers specified in b or B (can include SI prefix).", "1kiB"},
    )

  // TODO: Add packet types as needed
  SST_ELI_DOCUMENT_PORTS(
    {"port", "Port that connects to a Mordred router.", { "untimedMordredEvent", "basicMordredEvent" } },
  )

  SST_ELI_DOCUMENT_STATISTICS(
    {"packets_recv", "Number of packets received", "unitless", 3},
    {"average_noc_latency", "Average latency (in clocks) of each packet", "unitless", 3},
    {"average_packet_size", "Average packet size in number of flits", "unitless", 3}
    )

  /* For the constructor, int vns is an argument expected by SimpleNetwork interfaces;
   * we don't use said argument and the number of VNs is sent to us by the routers
   * during initialization.
   */

  MordredNIC( ComponentId_t cid, Params& params, int vns );
  ~MordredNIC() override { /* empty destructor */ }

  /// SST Required
  void init( uint32_t phase ) override;
  void setup() override;
  void complete( uint32_t phase ) override;
  void finish() override;

  /**
    * Sends a network request during the init() phase
    */
  void sendUntimedData( Request* req ) override;

  /**
   * Receive any data during the init() phase.
   */
  Request* recvUntimedData() override;

  /**
   * Send a Request to the network.
   */
  bool send( Request* req, int vn ) final;

  /**
   * Receive a Request from the network.
   *
   * Use this method for polling-based applications.
   * Register a handler for push-based notification of responses.
   *
   * @param vn Virtual network to receive on
   * @return NULL if nothing is available.
   * @return Pointer to a Request response (that should be deleted)
   */
  Request* recv( int vn ) override;

  /**
         * Checks if there is sufficient space to send on the specified
         * virtual network
         * @param vn Virtual network to check
         * @param num_bits Minimum size in bits required to have space
         * to send
         * @return true if there is space in the output, false otherwise
         */
  bool spaceToSend( int vn, int num_bits ) override;

  /**
         * Checks if there is a waiting network request request pending in
         * the specified virtual network.
         * @param vn Virtual network to check
         * @return true if a network request is pending in the specified
         * virtual network, false otherwise
         */
  bool requestToReceive( int vn ) override; // use with sst version 14

  /**
    * Registers a functor which will fire when a new request is
    * received from the network.  Note, the actual request that
    * was received is not passed into the functor, it is only a
    * notification that something is available.
    * @param functor Functor to call when request is received
  */
  void setNotifyOnReceive( SimpleNetwork::HandlerBase* functor ) override {
    receiveFunctor = functor;
  }

  /**
    * Registers a functor which will fire when a request is
    * sent to the network.  Note, this only tells you when data
    * is sent, it does not guarantee any specified amount of
    * available space.
    * @param functor Functor to call when request is sent
  */
  void setNotifyOnSend( SimpleNetwork::HandlerBase* functor ) override {
    sendFunctor = functor;
  }

  /*********** Full functions **************/
  bool isNetworkInitialized() const override { return initialized; }
  nid_t getEndpointID() const override { return netID; }
  const UnitAlgebra& getLinkBW() const override { return bw; }

  bool clockTick( Cycle_t cycle );

  /// default constructor
  MordredNIC() : Interfaces::SimpleNetwork() {}

  /// serialization
  void serialize_order(SST::Core::Serialization::serializer& ser) override {
    SST_SER(output);
    SST_SER(link);
    SST_SER(netID);
    SST_SER(rtrId);
    SST_SER(rtrPort);
    SST_SER(initialized);
    SST_SER(numVns);
    SST_SER(numVcs);
    SST_SER(flitSize);
    SST_SER(packetId);
    SST_SER(headInjectCycle);
    SST_SER(bw);
    SST_SER(inbufSize);
    SST_SER(outbufSize);
    SST_SER(initEvents);
    SST_SER(inBuf);
    SST_SER(outBuf);
    SST_SER(rtrCredits);
    SST_SER(outbufCredits);
    SST_SER(totalNocLatency);
    SST_SER(totalPackets);
    SST_SER(totalNumFlits);
    SST_SER(statPacketsRecv);
    SST_SER(statAvgNocLatency);
    SST_SER(statAvgFlitsPerPacket);
  }

  /// serialization implementations
  ImplementSerializable(SST::Mordred::MordredNIC);

private:
  void resizeVectors();
  MordredInitEvent* getInitEvent( MordredInitEvent::Commands cmd );
  int32_t           calcNumFlits( uint32_t num_bits );

  // event handlers
  void handleIncomingPacket( SST::Event* ev );

  Output*     output;
  Link*       link;
  nid_t       netID;
  uint32_t    rtrId{UINT32_MAX};
  uint32_t    rtrPort{UINT32_MAX};
  bool        initialized{false};
  uint32_t    numVns{0};
  uint32_t    numVcs{UINT32_MAX}; // Tracked, but unused
  uint32_t    flitSize{};
  uint64_t    packetId{};
  uint64_t    headInjectCycle{UINT64_MAX};

  UnitAlgebra bw;

  HandlerBase* sendFunctor{nullptr};
  HandlerBase* receiveFunctor{nullptr};

  // in bits
  UnitAlgebra inbufSize;
  UnitAlgebra outbufSize;

  std::queue<MordredInitEvent*> initEvents; // TODO: baseMordredEvent instead?

  // Note: All of the vectors below are sized to the number of VNs

  // Packet buffers
  std::vector<std::queue<Request*>> inBuf; // from router
  std::vector<std::queue<MordredFlit*>> outBuf; // to router

  // Credit counters; 1 credit = 1 flit
  // Note on credits: we send the router a number of credits equal to the number of flits
  // that the inBuf can hold.

  // credits received from router; initialization comes from router in init;
  // (decrement on send to router, increment when credit packet comes from router)
  std::vector<int32_t> rtrCredits;

  // credits for space in the outBuf (decrement as msgs recv'd from endpoint,
  // increment when put on network))
  std::vector<int32_t> outbufCredits;

  // Notes to self: NIC can accept a packet from the endpoint when there are
  // outbufCredits; this gets broken up into flits and stays in the outBuf
  // until there are rtrCredits available so we can send the packet

  // credits to return to the router as the inBuf is emptied out
  // init to 0
  std::vector<int32_t> inReturnCredits;

  // Statistics
  uint64_t totalNocLatency{0}; // in clock ticks
  uint64_t totalPackets{0};
  uint64_t totalNumFlits{0};
  Statistic<uint64_t>* statPacketsRecv;
  Statistic<double>* statAvgNocLatency;
  Statistic<double>* statAvgFlitsPerPacket;

};  // MordredNIC

}  // namespace SST::Mordred

#endif // MORDREDNIC_H
