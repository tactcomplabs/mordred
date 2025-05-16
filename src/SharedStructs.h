//
// SharedStructs.h
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef SHAREDSTRUCTS_H
#define SHAREDSTRUCTS_H

// Standard headers
#include <cinttypes>
#include <vector>

// Local SST header
#include "sst_config.h"

// Local headers
#include "MordredEvents.h"

namespace SST::Mordred {

// Fully expect this to be expanded as I develop the port control and arbitration;
// this is using c++ containers instead of c arrays for vc_heads in merlin
class InVcHeads {

public:
  explicit InVcHeads( uint32_t num_vcs ) { heads.resize( num_vcs, nullptr ); }

  std::vector<MordredFlit*> heads;
};

} //namespace SST::Mordred

#endif //SHAREDSTRUCTS_H
