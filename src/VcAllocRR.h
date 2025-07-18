//
// VcAllocRR.h
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef VCALLOCRR_H
#define VCALLOCRR_H

// Standard headers
#include <cstdint>

// Local SST header
#include "sst_config.h"

// Other local headers
#include "VcAllocAPI.h"
#include "MordredEvents.h"

namespace SST::Mordred {

class VcAllocRR : public VcAllocAPI {
public:
  SST_ELI_REGISTER_SUBCOMPONENT(
    VcAllocRR,
    "mordred",
    "VcAllocRR",
    SST_ELI_ELEMENT_VERSION( 0, 1, 0 ),
    "Round robin allocation for virtual channels",
    SST::Mordred::VcAllocAPI
  )

  // TODO: Use or delete this parameter - just auto set to 5 now in constructor
  SST_ELI_DOCUMENT_PARAMS( { "verbose", "Sets the output verbosity", "5" }, ) // currently unused

  SST_ELI_DOCUMENT_PORTS()

  SST_ELI_DOCUMENT_STATISTICS()

  VcAllocRR( ComponentId_t id, Params& params, uint32_t rtr_id, uint32_t num_ports, uint32_t num_vns, uint32_t num_vcs );

  ~VcAllocRR() final = default;

  // Lifecycle functions
  void init(unsigned int phase) final { /* empty */ }
  void setup() final { /* empty */ }
  void complete(unsigned int phase) override { /* empty */ }
  void finish() override { /* empty */ }

  // TODO: Add parameters as needed
  void arbitrate( std::vector<RtrPortControlAPI>& ports, std::vector<RtrOwnedSharedObjs>& rtr_shared_objs ) final;

  MordredFlit* findWinner( RtrOwnedSharedObjs* obj );

private:
  Output   *output;
  uint32_t rtrId;
  uint32_t numPorts{UINT32_MAX};
  uint32_t numVns{UINT32_MAX};
  uint32_t numVcs{UINT32_MAX};

  uint32_t next_rr_port{0};
  uint32_t next_rr_vn{0};
  uint32_t next_rr_vc{0};
  uint32_t vn_winner;
  uint32_t vc_winner;
  uint32_t next_out_rr_vc{0};
  uint32_t output_vc{UINT32_MAX};
};

} // namespace SST::Mordred

#endif //VCALLOCRR_H
