//
// XbarArbRR.h
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

// Changing this file up quite a bit - it was originally doing both VC and switch alloc, but
// now it's just going to be for switch allocation

#ifndef XBARARBRR_H
#define XBARARBRR_H

// Standard headers
#include <cstdint>

// Local SST header
#include "sst_config.h"

// Other local headers
#include "XbarArbAPI.h"
#include "MordredEvents.h"

namespace SST::Mordred {

class XbarArbRR : public XbarArbAPI {
public:
  SST_ELI_REGISTER_SUBCOMPONENT(
    XbarArbRR,
    "mordred",
    "xbarArbRR",
    SST_ELI_ELEMENT_VERSION( 0, 1, 0 ),
    "Round robin arbitration for the crossbar switch within the router",
    SST::Mordred::XbarArbAPI
  )

  // TODO: Use or delete this parameter - just auto set to 5 now in constructor
  SST_ELI_DOCUMENT_PARAMS( { "verbose", "Sets the output verbosity", "5" }, ) // currently unused

  SST_ELI_DOCUMENT_PORTS()

  SST_ELI_DOCUMENT_STATISTICS()

  XbarArbRR( ComponentId_t id, Params &params, uint32_t rtr_id, uint32_t num_ports, uint32_t num_vns, uint32_t num_vcs );

  ~XbarArbRR() final = default;

  void arbitrate( std::vector<RtrPortControlAPI*> &ports, std::vector<RtrOwnedSharedObjs> &rtr_shared_objs ) final;

private:
  Output   *output;
  uint32_t rtrId;
  uint32_t numPorts{UINT32_MAX};
  uint32_t numVns{UINT32_MAX};
  uint32_t numVcs{UINT32_MAX};

  uint32_t recv_rr_port{0}; // use to track rr start for receiving ports
  uint32_t send_rr_port{0}; // use to track rr start for sending ports
  uint32_t send_rr_vn{0};
  uint32_t send_rr_vc{0};
  uint32_t sending_vn;
  uint32_t sending_vc;

  void resetSendingVnVc() { sending_vn = sending_vc = UINT32_MAX; }
  bool findSendableFlit( uint32_t rcvportnum, RtrPortControlAPI* &sendport, RtrOwnedSharedObjs &shared_obj );

};

} // namespace SST::Mordred

#endif //XBARARBRR_H
