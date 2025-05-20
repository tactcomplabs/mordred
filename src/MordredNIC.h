//
// MordredNIC.h
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

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
    { "verbose",      "Sets the output verbosity", "5" },
    //{"link_bw",       "Bandwidth of the links specified in either b/s or B/s (can include SI prefix)."},
    //{"flit_size",     "Size of a flit in either b or B (can include SI prefix)."},
    { "input_buf_size",  "Size of input buffers specified in b or B (can include SI prefix).", "1kiB"},
    { "output_buf_size", "Size of output buffers specified in b or B (can include SI prefix).", "1kiB"}
    )

  // TODO: Add packet types as needed
  SST_ELI_DOCUMENT_PORTS(
    {"port", "Port that connects to a Mordred router.", { "untimedMordredEvent", "basicMordredEvent" } },
  )

  // TODO: Add stats
  SST_ELI_DOCUMENT_STATISTICS()

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
               * @see SST::Link::recvInitData()
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
  bool requestToReceive( int vn ) override;

  /**
    * Registers a functor which will fire when a new request is
    * received from the network.  Note, the actual request that
    * was received is not passed into the functor, it is only a
    * notification that something is available.
    * @param functor Functor to call when request is received
  */
  void setNotifyOnReceive( SimpleNetwork::HandlerBase* functor ) override {
    output->verbose(CALL_INFO, 5, 0, "MordredNIC Set recv-notify functor\n");
    recvFunctor = functor;
  }

  /**
    * Registers a functor which will fire when a request is
    * sent to the network.  Note, this only tells you when data
    * is sent, it does not guarantee any specified amount of
    * available space.
    * @param functor Functor to call when request is sent
  */
  void setNotifyOnSend( SimpleNetwork::HandlerBase* functor ) override {
    output->verbose(CALL_INFO, 5, 0, "MordredNIC Set send-notify functor\n");
    sendFunctor = functor;
  }

  /*********** Full functions **************/
  bool isNetworkInitialized() const override { return initialized; }
  nid_t getEndpointID() const override { return netID; }
  const UnitAlgebra& getLinkBW() const override { return bw; }

  bool clockTick( Cycle_t cycle );

private:
  void resizeVectors();
  MordredInitEvent* getInitEvent( MordredInitEvent::Commands cmd );

  // event handlers
  void handleIncomingPacket( SST::Event* ev );

  Output*     output;
  Link*       link;
  nid_t       netID;
  uint32_t    rtrId{UINT32_MAX};
  uint32_t    rtrPort{UINT32_MAX};
  bool        initialized{false};
  uint32_t    numVcs{UINT32_MAX};
  uint32_t    flitSize{};
  uint32_t    channelBusWidth{}; // TODO: Make UnitAlgebra if we're going to use it

  HandlerBase* sendFunctor{nullptr};
  HandlerBase* recvFunctor{nullptr};

  // in bits
  UnitAlgebra inbuf_size;
  UnitAlgebra outbuf_size;

  /*
   * Note: All of the vectors below have a length equal to the number of VCs instead of
   * the number of VNs as is done in merlin.  TODO: The question here is should our endpoint
   * NICs be dealing with/worrying about VNs/VCs?
   */

  // Packet buffers
  std::vector<std::queue<Request*>> in_buf; // from router
  std::vector<std::queue<Request*>> out_buf; // to router

  // Credit counters; 1 credit = 1 flit
  // credits received from router; initalization comes from router in init;
  // (decrement on send to router, increment when credit packet comes from router)
  std::vector<int32_t> rtr_credits;

  // credits for space in the out_buf (decrement as msgs recv'd from endpoint,
  // increment when put on network))
  std::vector<int32_t> outbuf_credits;

  // Notes to self: NIC can accept a packet from the endpoint when there are
  // outbuf_credits; this gets broken up into flits and stays in the outbuf
  // until there are rtr_credits available so we can send the packet

  // credits to return to the router as the in_buf is emptied out
  // init to 0
  std::vector<int32_t> in_ret_credits;

  UnitAlgebra bw; // Need?

};  // MordredNIC

}  // namespace SST::Mordred