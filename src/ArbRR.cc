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

using namespace SST::Mordred;

ArbRR::ArbRR( ComponentId_t id, Params &params, std::vector<std::vector<RtrOwnedVnObj>> *vn_objs,
  std::vector<std::pair<uint32_t,uint32_t>> *arb_winners) :
ArbAPI( id ),
vnObjs( vn_objs ),
arbWinners( arb_winners )
{
  const auto verbosity = params.find<uint32_t>("verbose", 5);
  output = new Output("ArbRR[" + getName() + ":@p:@t]: ", verbosity, 0, Output::STDOUT);

  numPorts = params.find<uint32_t>("num_ports", 3);

  if (vnObjs == nullptr) {
    output->fatal( CALL_INFO, -1, "vn_objs is a nullptr\n" );
  }

  if (vnObjs->size() != numPorts) {
    output->fatal(CALL_INFO, -1, "Number of ports in vn_objs does not match number of ports in router\n");
  }
}

void ArbRR::arbitrate( ) {
  if (vnObjs == nullptr) {
    // TODO: sanity check for now...delete eventually
    output->fatal( CALL_INFO, -1, "vc_heads is a nullptr\n" );
  }
  // reset arbWinners; this assumes that we're ok re-arbitrating everything each cycle
  std::fill( arbWinners->begin(), arbWinners->end(), std::make_pair( UINT32_MAX, UINT32_MAX ) );

  for ( uint32_t i = 0; i < numPorts; i++) {
    if ( vnObjs->at(i).empty() ) // unconnected ports
      continue;

    // j = Per VN loop
    for ( uint32_t j = 0; j < vnObjs->at(i).size(); j++ ) {
      // k = per VC loop
      auto &vcHeads = vnObjs->at(i).at(j).vcHeads;
      for ( uint32_t k = 0; k < vcHeads.size(); k++ ) {
        auto flit = vcHeads.at( k );
        if ( flit != nullptr ) {
          if ( vnObjs->at( flit->next_port ).empty() )
            output->fatal( CALL_INFO, -1, "Flit next_port == %" PRIu32 " is unconnected\n", flit->next_port );
          auto *simple = static_cast<simpleTestEvent*>( flit->req->inspectPayload() );
          output->verbose( CALL_INFO, 5, 0, "Port %" PRIu32 " has a packet with str=%s\n", i, simple->str.c_str() );
          // only moving a single flit right now, so i can just assume it happens - clearly will need to check the output
          // side of the dest port to ensure it can move through
          // TODO: get VN,VC from packet
          arbWinners->at(i) = std::make_pair( 0, 0 ); // 0,0 is the VN,VC of the receiving port for the flit
        }
      } // end k loop
    } // end j loop
  } // end i loop

}
