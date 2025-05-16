//
// ArbRR.cc
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//
//

#include <string>
#include <cinttypes>

// Local SST config
#include "sst_config.h"

#include "MordredEvents.h"
#include "ArbRR.h"
#include "RtrPortControl.h"
#include "TopologyAPI.h"

using namespace SST::Mordred;

ArbRR::ArbRR( ComponentId_t id, Params &params, std::vector<std::vector<MordredFlit*>> *vc_heads  ) :
ArbAPI( id ),
vcHeads( vc_heads )
{
  const auto verbosity = params.find<uint32_t>("verbose", 5);
  output = new SST::Output("ArbRR[" + getName() + ":@p:@t]: ", verbosity, 0, Output::STDOUT);

  numPorts = params.find<uint32_t>("num_ports", 3);
  numVcs = params.find<uint32_t>( "num_vcs", 1 );

  if (vcHeads == nullptr) {
    output->fatal( CALL_INFO, -1, "vc_heads is a nullptr\n" );
  }

  if (vcHeads->size() != numPorts) {
    output->fatal(CALL_INFO, -1, "Number of ports in vc_heads does not match number of ports in router\n");
  }
}

void ArbRR::arbitrate( ) {
  if (vcHeads == nullptr) {
    output->fatal( CALL_INFO, -1, "vc_heads is a nullptr\n" );
  }

  for ( next_port = 0; next_port < numPorts; next_port++) {
    if ( vcHeads->at(next_port).empty() ) { // unconnected ports
      continue;
    }
    auto flit = vcHeads->at(next_port).at(0);
    if ( flit != nullptr ) {
      auto *simple = static_cast<simpleTestEvent*>( flit->req->inspectPayload() );
      output->verbose( CALL_INFO, 5, 0, "Port %" PRIu32 " has a packet with str=%s\n", next_port, simple->str.c_str() );
    }
  }
}
