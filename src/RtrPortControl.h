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

namespace SST::Mordred {

/**
 * Miscellaneous Notes:
 *  - If this is an ENDPOINT port, it really only needs one VC; we don't pass that knowledge into
 *    the object and configure it that way though (would likely require changes to SimpleRTR)
 *  - Only connected ports should ever be created (via the SimpleRtr constructor) so we don't
 *      do any checking for that here
 *  - For a given clock tick, if all of the output buffers are empty/blocked from sending,
 *      then this module will try to return credits using a round-robin approach.
 */

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

  // All of the parameters are handled/passed in from the SimpleRtr
  SST_ELI_DOCUMENT_PARAMS(
    //{ "verbose", "Sets the output verbsoity", "5" },
    //{"link_bw",       "Bandwidth of the links specified in either b/s or B/s (can include SI prefix)."},
    //{"flit_size",     "Size of a flit in either b or B (can include SI prefix)."},
    //{ "link_bw",        "Bandwidth of the links specified in either b/s or B/s (can include SI prefix)."},
    //{ "in_buf_size",    "Size of input buffers specified in b or B (can include SI prefix).", "1kB"},
    //{ "out_buf_size",   "Size of output buffers specified in b or B (can include SI prefix).", "1kB"}
    )

  // Use the parent ports -- assume anonymous loading
  SST_ELI_DOCUMENT_PORTS()

  SST_ELI_DOCUMENT_STATISTICS()

  RtrPortControl( ComponentId_t id, Params& params, TopologyAPI* topology, std::vector<RtrOwnedVnObj>* vn_objs, uint32_t rtr_num, uint32_t port_num );

  ~RtrPortControl() final = default;

  // Lifecycle functions
  void init(unsigned int phase) final;
  void setup() final;
  void complete(unsigned int phase) override { /* empty */ }
  void finish() override { /* empty */ }

  void sendUntimedData(Event* ev) final;
  Event* recvUntimedData() final;

  void ClockTick(Cycle_t cycle) final;

  void inHandler(Event* ev);

  // Switch/xbar interactions
  int32_t getOutBufCreditCount( std::pair<uint32_t, uint32_t> vn_vc ) final {
    return outBufCredits[vn_vc.first][vn_vc.second];
  }
  MordredFlit* getInBufFlit( std::pair<uint32_t, uint32_t> vn_vc ) final;
  void   sendOutBufFlit( MordredFlit* flit, std::pair<uint32_t, uint32_t> vn_vc ) final; // Rename?

private:
  void allocateBuffers();
  MordredInitEvent* getInitEvent( MordredInitEvent::Commands cmd );
  void returnCredit();

  Output* output;
  Link*   link{};
  TopologyAPI *topo{};
  PortConnectionE connectionType{UNKNOWN};
  uint32_t rtrId;
  uint32_t portId;
  uint32_t connectedRtrId{UINT32_MAX};
  uint32_t connectedPortId{UINT32_MAX};
  uint32_t numVns{UINT32_MAX}; // from size of vn_objs
  uint32_t numVcs{UINT32_MAX}; // from size of vector in vn_objs
  uint32_t flitSize{}; // in bits
  uint32_t channelBusWidth{}; // in bits
  uint32_t flit_vn_rr{};
  uint32_t flit_vc_rr{};
  uint32_t credit_ret_vn_rr{};
  uint32_t credit_ret_vc_rr{};

  UnitAlgebra param_link_bw;
  UnitAlgebra param_flit_size;

  // These are in bits
  uint32_t inBufSize;
  uint32_t outBufSize;

  // Each element is for a VN
  std::vector<RtrOwnedVnObj> *perVnObjs{};

  /**
   * For the structures below, the outer dimension is VN, inner is VC
   */

  // Not using these, but these will probably be necessary if we don't want
  // to rearbitrate every cycle
  std::vector<std::vector<InVcStateE>> inStates;
  std::vector<std::vector<OutVcStateE>> outStates;

  // Packet buffers
  // Note: For now, we have a separate output buffer for each VN/VC combo; this
  // would allow for us to
  std::vector<std::vector<std::queue<MordredFlit*>>> inBuf; // from router/endpt
  std::vector<std::vector<std::queue<MordredFlit*>>> outBuf; // to router/endpt

  // Credit counters; 1 credit = 1 flit
  // credits received from destination; initialized to non-zero in init (dest sends a count)
  // (dec on send to dest, inc when credit packet comes from dest)
  std::vector<std::vector<int32_t>> destCredits;

  // credits for space in the outBuf (decrement as flits inserted,
  // increment when put on link) - purely internal (and
  // unnecessary if outBuf is removed)
  // initialize to outBufSize
  std::vector<std::vector<int32_t>> outBufCredits;

  // credits to return to the sender as the inBuf is emptied out
  // init to zero
  std::vector<std::vector<int32_t>> inRetCredits;

};

} // namespace SST::Mordred
#endif //RTRPORTCONTROL_H
