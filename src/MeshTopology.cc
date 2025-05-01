//
// MeshTopology.cc
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

// Standard headers
#include <cstdint>
#include <cinttypes>

// Local SST header
#include "sst_config.h"

// Local headers
#include "MeshTopology.h"

using namespace SST::Mordred;

MeshTopology::MeshTopology( ComponentId_t id, Params& params,
  ComponentId_t rtr_id_, uint32_t num_rtr_ports, uint32_t num_local_ports ) :
  TopologyAPI( id ),
  rtr_id( rtr_id_ ),
  rtr_port_count( num_rtr_ports ),
  local_port_count( num_local_ports )
{
  const auto verbosity = params.find<uint32_t>( "verbose", 5 );
  output               = new Output( "MeshTopology [" + getName() + ":@p:@t]:", verbosity, 0, Output::STDOUT );

  // Process and validate input parameters
  xId                  = params.find<int32_t>( "xId", -1 );
  yId                  = params.find<int32_t>( "yId", -1 );
  xSize                = params.find<int32_t>( "xSize", 1 );
  ySize                = params.find<int32_t>( "ySize", 1 );

  if( ( xId < 0 ) || ( yId < 0 ) ) {
    output->fatal( CALL_INFO, -1, "Invalid xId=%" PRId32 " or yId=%" PRId32 "\n", xId, yId );
  }

  if( ( xId >= xSize ) || ( yId >= ySize ) ) {
    output->fatal(
      CALL_INFO, -1, "xId=%" PRId32 " >= xSize=%" PRId32 " OR yId=%" PRId32 " >= ySize=%" PRId32 "\n", xId, xSize, yId, ySize
    );
  }

  num_links = 4;
  if( xId == 0 )
    num_links--;
  if( xId == ( xSize - 1 ) )
    num_links--;
  if( yId == 0 )
    num_links--;
  if( yId == ( ySize - 1 ) )
    num_links--;

  dir_topo_port_vec.assign( 4, -1 );

  output->verbose( CALL_INFO, 1, 0, "MeshTopology constructed; num_links=%" PRIu32 "\n", num_links );
  output->verbose( CALL_INFO, 1, 0, "\t rtr_id=%" PRIu64 ", rtr_ports=%" PRIu32 ", local_ports=%" PRIu32 "\n",
    rtr_id, rtr_port_count, local_port_count);
}

void MeshTopology::init( uint32_t phase ) {
  output->verbose( CALL_INFO, 1, 0, "MeshTopology, init function\n" );
  output->flush();

  switch( init_state ) {
  case 0: {
    for ( uint32_t i = 0; i < num_links; i++ ) {
      auto *bev = new MordredFlit();
      bev->src_name = "rtr_" + std::to_string( xId ) + "_" + std::to_string( yId );
      init_out_queue.push( bev );
    }
    init_state = 1;
    break;
  }
  case 1: [[fallthrough]];


  case 2: [[fallthrough]];
  default:
    output->fatal( CALL_INFO, -1, "Not implemented\n" );

  }
}


MordredFlit* MeshTopology::sendInitMessage() {

  if ( init_out_queue.empty() )
    return nullptr;

  auto *bev = init_out_queue.front();
  init_out_queue.pop();
  return bev;
}

void MeshTopology::processInitMessage( Event* ev, size_t topo_port_num, uint32_t vn=1 ) {

  //init_in_queue.push( std::make_tuple( ev, topo_port_num, vn ) );
  output->flush();
  //output->fatal( CALL_INFO, -1, "Not yet implemented\n" );
  output->verbose( CALL_INFO, 1, 0, "Not yet implemented\n" );
  auto bev = static_cast<MordredFlit*>(ev);
  output->verbose( CALL_INFO, 5, 0, "%s Rcvd init message on port=%zu with msg=%s\n",
    getName().c_str(), topo_port_num, bev->src_name.c_str());

  // Map the incoming message to the dir_topo_port_vec
  std::string nStr = "rtr_" + std::to_string( xId ) + "_" + std::to_string( yId + 1 );
  std::string sStr = "rtr_" + std::to_string( xId ) + "_" + std::to_string( yId - 1 );
  std::string eStr = "rtr_" + std::to_string( xId + 1 ) + "_" + std::to_string( yId );
  std::string wStr = "rtr_" + std::to_string( xId - 1 ) + "_" + std::to_string( yId );

  output->verbose(CALL_INFO, 5, 0, "Strings: %s, %s, %s, %s\n",
    nStr.c_str(), eStr.c_str(), sStr.c_str(), wStr.c_str() );

  if ( 0 == strcmp( nStr.c_str(), bev->src_name.c_str() ) )
    dir_topo_port_vec[0] = static_cast<int32_t>( topo_port_num );
  else if ( 0 == strcmp( eStr.c_str(), bev->src_name.c_str() ) )
    dir_topo_port_vec[1] = static_cast<int32_t>( topo_port_num );
  else if ( 0 == strcmp( sStr.c_str(), bev->src_name.c_str() ) )
    dir_topo_port_vec[2] = static_cast<int32_t>( topo_port_num );
  else if ( 0 == strcmp( wStr.c_str(), bev->src_name.c_str() ) )
    dir_topo_port_vec[3] = static_cast<int32_t>( topo_port_num );

  output->verbose( CALL_INFO, 5, 0, "\t PV=[%d,%d,%d,%d]\n",
    dir_topo_port_vec[0], dir_topo_port_vec[1], dir_topo_port_vec[2], dir_topo_port_vec[3]);

}
