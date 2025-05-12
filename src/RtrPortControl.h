//
// RtrPortControl.h
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//
//

#ifndef RTRPORTCONTROL_H
#define RTRPORTCONTROL_H

// Local SST config
#include "sst_config.h"

#include "MordredEvents.h"
#include "RtrPortControlAPI.h"
#include "TopologyAPI.h"

namespace SST::Mordred {

class RtrPortControl : public RtrPortControlAPI {
public:
  SST_ELI_REGISTER_SUBCOMPONENT(
    RtrPortControl,
    "mordred",
    "rtrPortControl",
    SST_ELI_ELEMENT_VERSION( 0, 1, 0 ),
    "Manage a port on the Mordred router",
    SST::Mordred::RtrPortControlAPI
  )

  SST_ELI_DOCUMENT_PARAMS(
    { "verbose", "Sets the output verbsoity", "5" },
    //{"link_bw",       "Bandwidth of the links specified in either b/s or B/s (can include SI prefix)."},
    //{"flit_size",     "Size of a flit in either b or B (can include SI prefix)."},
    //{ "link_bw",        "Bandwidth of the links specified in either b/s or B/s (can include SI prefix)."},
    //{ "in_buf_size",    "Size of input buffers specified in b or B (can include SI prefix).", "1kB"},
    //{ "out_buf_size",   "Size of output buffers specified in b or B (can include SI prefix).", "1kB"}
    )

  // Use the parent ports -- assume anonymous loading
  SST_ELI_DOCUMENT_PORTS()

  SST_ELI_DOCUMENT_STATISTICS()

  RtrPortControl( ComponentId_t id, Params& params, TopologyAPI* topology, uint32_t rtr_num, uint32_t port_num );

  ~RtrPortControl() final = default;

  // Lifecycle functions
  void init(unsigned int phase) final;
  void setup() final;
  void complete(unsigned int phase) override { /* empty */ }
  void finish() override { /* empty */ }

  void sendUntimedData(Event* ev) final;
  Event* recvUntimedData() final;

  void inHandler(SST::Event* ev);

private:
  MordredInitEvent* getInitEvent( MordredInitEvent::Commands cmd );

  Output* output;
  Link*   link{};
  TopologyAPI *topo{};
  PortConnectionE connectionType{UNKNOWN};
  uint32_t rtrId;
  uint32_t portId;
  uint32_t connectedRtrId{UINT32_MAX};
  uint32_t connectedPortId{UINT32_MAX};
  uint32_t numVcs{};
  uint32_t flitSize{}; // in bits
  uint32_t channelBusWidth{}; // in bits


  UnitAlgebra param_link_bw;
  UnitAlgebra param_flit_size;

  // These are in bits
  uint32_t inbuf_size;
  uint32_t outbuf_size;

  // Packet buffers
  std::vector<std::queue<MordredFlit*>> in_buf; // from router/endpt
  std::vector<std::queue<MordredFlit*>> out_buf; // to router/endpt - NEED?

  // Credit counters; 1 credit = 1 flit
  // credits received from destination; initialized to non-zero in init (dest sounds a count)
  // (dec on send to dest, inc when credit packet comes from dest)
  std::vector<int32_t> dest_credits;

  // credits for space in the out_buf (decrement as flits inserted,
  // increment when put on link) - purely internal (and
  // unnecessary if out_buf is removed)
  // initialize to outbuf size
  std::vector<int32_t> outbuf_credits;

  // credits to return to the sender as the in_buf is emptied out
  // init to zero
  std::vector<int32_t> in_ret_credits;

};

} // namespace SST::Mordred
#endif //RTRPORTCONTROL_H
