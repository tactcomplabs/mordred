//
// VcAllocRR.cc
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#include "VcAllocRR.h"

using namespace SST::Mordred;

VcAllocRR::VcAllocRR( ComponentId_t id, Params& params, uint32_t rtr_id, uint32_t num_ports, uint32_t num_vns, uint32_t num_vcs ) :
  VcAllocAPI( id ),
  rtrId( rtr_id ),
  numPorts( num_ports ),
  numVns( num_vns ),
  numVcs( num_vcs )
{
  const auto verbosity = params.find<uint32_t>("verbose", 5);
  output = new Output("RtrPortControl[[" + std::to_string( rtrId ) + "]:@p:@t]: ",
    verbosity, 0, Output::STDOUT);
}

// TODO: WE SHOULD NOT BE UPDATING THE VN THROUGH HERE!!!
void VcAllocRR::arbitrate( std::vector<RtrPortControlAPI>& ports, std::vector<RtrOwnedSharedObjs>& rtr_shared_objs ) {
  for( uint32_t i = 0, portnum = next_rr_port; i < numPorts; ++i, portnum = ( portnum + 1 ) % numPorts ) {
    auto& shared_obj  = rtr_shared_objs[portnum];
    // I need to know if there is a VN, VC pair in this shared object that needs an output VN, VC
    MordredFlit* flit = findWinner( &shared_obj );
    if( flit != nullptr ) {
      // Have an input port, vn, vc combo that wants a VN, VC at a given output port (which the input port knows)
      auto out_port = ports.at(portnum).getOutPort( vn_winner, vc_winner );
      if ( out_port >= numPorts )
        output->fatal( CALL_INFO, -1, "Invalid out_port=%" PRIu32 "\n", out_port );
      // Now, need to find the next available VC for this VN for this output port
      auto winning_out_vc = ports.at(out_port).assignOutVc( vn_winner, next_out_rr_vc );
      if ( winning_out_vc != UINT32_MAX ) {
        // Have a valid input, output pair - update both
        ports.at(portnum).inUnitSetOutputVc( vn_winner, flit->cur_vc, winning_out_vc );
        ports.at(out_port).outUnitSetInputVc( portnum, vn_winner, winning_out_vc );
        shared_obj.needVcAlloc.at( vn_winner ).at( vc_winner ) = nullptr;
      }
    }
  }
  // Update start values
  next_rr_port = ( next_rr_port + 1 ) % numPorts;
  next_rr_vn = ( next_rr_vn + 1 ) % numVns;
  next_rr_vc = ( next_rr_vc + 1 ) % numVcs;
  next_out_rr_vc = ( next_out_rr_vc + 1 ) % numVcs;
}

MordredFlit* VcAllocRR::findWinner( RtrOwnedSharedObjs* obj ) {
  for ( uint32_t i = 0, cur_vn = next_rr_vn; i < numVns; ++i, cur_vn = (cur_vn + 1) % numVns ) {
    for ( uint32_t j = 0, cur_vc = next_rr_vc; j < numVcs; ++j, cur_vc = (cur_vc + 1) % numVcs ) {
      if ( obj->needVcAlloc.at(cur_vn).at(cur_vc) != nullptr ) {
        vn_winner = cur_vn;
        vc_winner = cur_vc;
        return obj->needVcAlloc.at(cur_vn).at(cur_vc);
      }
    }
  }
  return nullptr;
}
