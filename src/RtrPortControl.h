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
 * Have a per VN,VC structure in RtrPortControlAPI that mostly models an input or output
 * unit in the Dally book.  It's owned here and we'll pass along the reference to the port
 * vector to whichever units need it from the router
 *
 * For now, let's do routing when we get a head flit at the head of a queue;
 * we're going to assume
 * that we have enough logic so that we don't have to worry about arbitrating
 * access to the router itself
 *
 */

/**
 * Miscellaneous Notes:
 *  - If this is an ENDPOINT port, it really only needs one VC; we don't pass that knowledge into
 *    the object and configure it that way though (would be pretty easy since we're passing in the
 *    full params struct)
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

  SST_ELI_DOCUMENT_STATISTICS(
  {"in_flit_cnt", "Number of incoming flits", "unitless", 3},
  )

  RtrPortControl( ComponentId_t id, Params& params, TopologyAPI* topology, RtrOwnedSharedObjs* rtr_shared_objs, uint32_t rtr_num, uint32_t port_num );

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

  // VC Alloc interactions
  uint32_t getOutPort( uint32_t vn, uint32_t vc ) final { return inStateVec.at(vn).at(vc).outPort; }

  // TODO: This should be in VcAllocRR since that's the unit that should be selecting an output VC based
  // on it's internal algo
  uint32_t assignOutVc( uint32_t vn, uint32_t start_vc ) final {
    for ( uint32_t i = 0, cur_vc = start_vc; i < numVcs; i++, cur_vc = ( cur_vc+1 ) % numVcs ) {
      if ( outStateVec.at(vn).at(cur_vc).outVcState == OUT_IDLE )
        return cur_vc;
    }
    return UINT32_MAX;
  }

  void inUnitSetOutputVc( uint32_t vn, uint32_t input_vc, uint32_t output_vc ) final {
    inStateVec.at(vn).at(input_vc).outVn = vn;
    inStateVec.at(vn).at(input_vc).outVc = output_vc;
  }

  void outUnitSetInputVc( uint32_t vn, uint32_t input_vc, uint32_t output_vc ) final {
    outStateVec.at(vn).at(output_vc).inVn = vn;
    outStateVec.at(vn).at(output_vc).inVc = input_vc;
  }


  // Switch/xbar interactions
  int32_t getOutBufCreditCount( std::pair<uint32_t, uint32_t> vn_vc ) final {
    return outStateVec.at( vn_vc.first ).at( vn_vc.second ).outBufCredits;
  }

  MordredFlit* getInBufFlit( std::pair<uint32_t, uint32_t> vn_vc ) final;
  void   recvOutBufFlit( MordredFlit* flit, std::pair<uint32_t, uint32_t> vn_vc ) final; // Rename?

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

  // TODO: Probably want a "port" state that IDs if something is already on the
  // move in/out of here (may be faster than searching states) - this may be
  // better at the router level

  // These are in bits
  uint32_t inBufSize;
  uint32_t outBufSize;

  // These are 2D - [vn][vc]
  std::vector<std::vector<perVcInState>> inStateVec;
  std::vector<std::vector<perVcOutState>> outStateVec;

  // Each element is for a VN
  RtrOwnedSharedObjs *rtrSharedObjs{};

  // Statistics
  // For the 2D vectors, the outer dimension is VN, inner is VC
  // Soooo, this keeps showing up as a NullStatistic after I try to register
  // it; no idea why at this point.  Seems like the SST docs are insufficient.
  std::vector<std::vector<Statistic<uint64_t>*>> statInFlitCnt;
};

} // namespace SST::Mordred
#endif //RTRPORTCONTROL_H
