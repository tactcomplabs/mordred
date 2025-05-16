//
// ArbAPI.h
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef ARBAPI_H
#define ARBAPI_H

// Standard headers
#include <cstdint>

// Local SST header
#include "sst_config.h"

// Other local headers
#include "MordredEvents.h"
#include "RtrPortControlAPI.h"

/**
 * This API is for the crossbar arbitration that happens within the router; in Merlin, this
 * is a pretty minimal API (XbarArbitration in router.h)
 */
namespace SST::Mordred {

class ArbAPI : public SubComponent {
  public:
    SST_ELI_REGISTER_SUBCOMPONENT_API( SST::Mordred::ArbAPI, std::vector<std::vector<MordredFlit*>>* );

  /// ArbAPI: constructor
  ArbAPI( ComponentId_t id ) : SubComponent( id ) {}

  /// ArbAPI: default destructor
  ~ArbAPI() override = default;

  /// Main arbitration function
  virtual void arbitrate( ) = 0; // TODO: Add arguments as needed

}; // class ArbAPI

} // namespace SST::Mordred

#endif //ARBAPI_H
