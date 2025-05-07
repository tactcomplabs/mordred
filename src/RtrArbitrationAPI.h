//
// RtrPortControlAPI.h
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef RTRARBITRATIONAPI_H
#define RTRARBITRATIONAPI_H


// Local SST config
#include "sst_config.h"

#include "MordredEvents.h"
#include "TopologyAPI.h"

/*
 * In Merlin, the XbarArbitration component is in router.h; multiple arbitration are
 * defined as subcomponents (default is xbar_arb_lru). Beyond arbitrating the hr_router
 * internal crossbar, this API also plays a role in turning the router clocking on/off
 *
 * Note: haven't reviewed any of the arbitration units as of yet
 */

namespace SST::Mordred {

class RtrArbitrationAPI : public SubComponent {
public:
  SST_ELI_REGISTER_SUBCOMPONENT_API( SST::Mordred::RtrArbitrationAPI )

  /// RtrArbitrationAPI: constructor
  RtrArbitrationAPI( ComponentId_t id ) : SubComponent( id ) {}

  /// RtrArbitrationAPI: default destructor
  ~RtrArbitrationAPI() override = default;

};  // class RtrArbitrationAPI

}  // namespace SST::Mordred


#endif //RTRARBITRATIONAPI_H
