//
// ArbRR.h
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef ARBRR_H
#define ARBRR_H

// Standard headers
#include <cstdint>

// Local SST header
#include "sst_config.h"

// Other local headers
#include "ArbAPI.h"
#include "MordredEvents.h"

namespace SST::Mordred {

class ArbRR : public ArbAPI {
public:
  SST_ELI_REGISTER_SUBCOMPONENT(
    ArbRR,
    "mordred",
    "arbRR",
    SST_ELI_ELEMENT_VERSION( 0, 1, 0 ),
    "Round robin arbitration for the router crossbar",
    SST::Mordred::ArbAPI
  )

  // TODO: Use or delete this parameter - just auto set to 5 now in constructor
  SST_ELI_DOCUMENT_PARAMS( { "verbose", "Sets the output verbosity", "5" }, ) // currently unused

  SST_ELI_DOCUMENT_PORTS()

  SST_ELI_DOCUMENT_STATISTICS()

  ArbRR( ComponentId_t id, Params &params, std::vector<std::vector<MordredFlit*>> *vc_heads,
    std::vector<uint32_t> *arb_winners);

  ~ArbRR() final = default;

  // I'm sure more parameters will be necessary
  void arbitrate( ) final;

private:
  Output   *output;
  uint32_t numPorts{UINT32_MAX};
  uint32_t numVcs{UINT32_MAX};

  std::vector<std::vector<MordredFlit*>> *vcHeads; // port_num.vc_num
  std::vector<uint32_t> *arbWinners; // index is port, contents is VC of who won arbitration

  //uint32_t next_port{0}; // use to track rr start
};

} // namespace SST::Mordred

#endif //ARBRR_H
