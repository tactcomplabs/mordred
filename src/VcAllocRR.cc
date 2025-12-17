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
  output = new Output("VcAllocRR[" + std::to_string( rtrId ) + ":@p:@t]: ",
    verbosity, 0, Output::STDOUT);

  resetSrcVnVc();
}

void VcAllocRR::arbitrate( std::vector<RtrPortControlAPI*>& ports, std::vector<RtrOwnedSharedObjs>& rtr_shared_objs ) {
  for( uint32_t i = 0, portnum = rr_port; i < numPorts; ++i, portnum = ( portnum + 1 ) % numPorts ) {
    if ( ports.at(portnum) == nullptr )
      continue;
    resetSrcVnVc();
    auto& shared_obj  = rtr_shared_objs[portnum];
    MordredFlit* flit = findMappableFlit( &shared_obj );
    if( flit != nullptr ) {
      auto& input_port = ports[portnum];
      auto dest_portnum = input_port->getDestPort( src_vn, src_vc );
      if ( ( dest_portnum >= numPorts ) || ( ports.at(dest_portnum) == nullptr ) )
        output->fatal( CALL_INFO, -1, "Invalid out_port=%" PRIu32 "; invalid packet dest=%" PRIu64 "?\n",
          dest_portnum,  flit->req->dest );
      auto dest_vc = findDestVc( ports.at( dest_portnum ) );
      if ( dest_vc != UINT32_MAX ) {
        input_port->inUnitSetDestVc( src_vn, src_vc, dest_vc );
        ports.at(dest_portnum)->outUnitSetSrc( portnum, src_vn, src_vc, dest_vc );
        shared_obj.needVcAlloc.at( src_vn ).at( src_vc ) = nullptr;
        //output->verbose( CALL_INFO, 5, 0, "Routed flit [Port:VN:VC] from [%" PRIu32 ":%" PRIu32 ":%" PRIu32 "] to [%" PRIu32 ":%" PRIu32 ":%" PRIu32 "]\n",
        //  portnum, src_vn, src_vc, dest_portnum, src_vn, dest_vc);
      }
    }
  }
  // Update start values
  rr_port = ( rr_port + 1 ) % numPorts;
  rr_vn = ( rr_vn + 1 ) % numVns;
  rr_vc = ( rr_vc + 1 ) % numVcs;
  rr_dest_vc = ( rr_dest_vc + 1 ) % numVcs;
}

// For a given port, determine if there's a flit that needs to be mapped
MordredFlit* VcAllocRR::findMappableFlit( RtrOwnedSharedObjs* obj ) {
  for ( uint32_t i = 0, cur_vn = rr_vn; i < numVns; ++i, cur_vn = (cur_vn + 1) % numVns ) {
    for ( uint32_t j = 0, cur_vc = rr_vc; j < numVcs; ++j, cur_vc = (cur_vc + 1) % numVcs ) {
      if ( obj->needVcAlloc.at(cur_vn).at(cur_vc) != nullptr ) {
        src_vn = cur_vn;
        src_vc = cur_vc;
        return obj->needVcAlloc.at(cur_vn).at(cur_vc);
      }
    }
  }
  return nullptr;
}

// For this port and VN, see if there's an IDLE VC; if not, we can't map this packet
uint32_t VcAllocRR::findDestVc( RtrPortControlAPI* &port ) const {
  if ( port->getConnectionType() == RtrPortControlAPI::ENDPOINT ) {
    if ( port->getOutputState( src_vn, 0 ) == OUT_IDLE )
      return 0;
  } else {
    for ( uint32_t i = 0, cur_vc = rr_dest_vc; i < numVcs; i++, cur_vc = ( cur_vc+1 ) % numVcs ) {
      if ( port->getOutputState( src_vn, cur_vc ) == OUT_IDLE )
        return cur_vc;
    }
  }
  return UINT32_MAX;
}