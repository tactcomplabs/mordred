//
// VcAllocRR.h
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

// Notes:
// - This will not modify the VN of any flit that goes through this allocator
// - This allocator is just looking for an IDLE destination (output) VC; not checking
//   credits anywhere

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

  void arbitrate( std::vector<RtrPortControlAPI*>& ports, std::vector<RtrOwnedSharedObjs>& rtr_shared_objs ) final;

private:
  Output   *output;
  uint32_t rtrId;
  uint32_t numPorts{UINT32_MAX};
  uint32_t numVns{UINT32_MAX};
  uint32_t numVcs{UINT32_MAX};

  uint32_t rr_port{0};
  uint32_t rr_vn{0};
  uint32_t rr_vc{0};
  uint32_t src_vn;
  uint32_t src_vc;
  uint32_t rr_dest_vc{0};

  void resetSrcVnVc() { src_vn = UINT32_MAX; src_vc = UINT32_MAX; }
  MordredFlit* findMappableFlit( RtrOwnedSharedObjs* obj );
  uint32_t findDestVc( RtrPortControlAPI* &port ) const;

};

} // namespace SST::Mordred

#endif //VCALLOCRR_H
