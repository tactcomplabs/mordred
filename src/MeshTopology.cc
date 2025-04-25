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

// Local SST header
#include "sst_config.h"

// Local headers
#include "MeshTopology.h"

using namespace SST::Mordred;

MeshTopology::MeshTopology( ComponentId_t id, Params& params ) : TopologyAPI( id, params ) {
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

  for ( uint32_t i = 0; i < num_links; i++ ) {
    auto *bev = new MordredFlit();
    bev->src_name = "rtr_" + std::to_string( xId ) + "_" + std::to_string( yId );
    init_flit_vec.push( bev );
  }

  dir_topo_port_vec.assign( 4, -1 );

  output->verbose( CALL_INFO, 1, 0, "MeshTopology constructed; num_links=%" PRIu32 "\n", num_links );
}

MordredFlit* MeshTopology::sendInitMessage() {

  if ( init_flit_vec.empty() )
    return nullptr;

  auto *bev = init_flit_vec.front();
  init_flit_vec.pop();
  return bev;
}

void MeshTopology::processInitMessage( size_t topo_port_num, Event* ev ) {
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
