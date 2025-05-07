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
  uint32_t rtr_id, uint32_t num_ports, uint32_t num_local_ports ) :
  TopologyAPI( id ),
  rtrId( rtr_id ),
  numPorts( num_ports ),
  numLocalPorts( num_local_ports )
{
  const auto verbosity = params.find<uint32_t>( "verbose", 5 );
  output               = new Output( "MeshTopology [" + getName() + ":@p:@t]:", verbosity, 0, Output::STDOUT );

  // Process and validate input parameters
  xDim                = params.find<uint32_t>( "xDim", 1 );
  yDim                = params.find<uint32_t>( "yDim", 1 );

  xId = rtrId % xDim;
  yId = rtrId / xDim;

  if( ( xId >= xDim ) || ( yId >= yDim ) ) {
    output->fatal(
      CALL_INFO, -1, "xId=%" PRId32 " >= xSize=%" PRId32 " OR yId=%" PRId32 " >= ySize=%" PRId32 "\n", xId, xDim, yId, yDim
    );
  }

  dir_topo_port_vec.assign( 4, -1 );

  output->verbose( CALL_INFO, 1, 0, "MeshTopology constructed; rtr_id=%" PRIu32 ", num_ports=%" PRIu32 ", local_ports=%" PRIu32 "\n",
    rtrId, numPorts, numLocalPorts);
}

TopologyAPI::PortConnectionE MeshTopology::getPortConnection( uint32_t portnum ) {
  if (portnum >= 4)
    return ENDPT;
  return ROUTER;
}

int32_t MeshTopology::getEndpointId( uint32_t portnum ) {
  if ( portnum < 4 )
    return -1;
  uint32_t base_id = rtrId * numLocalPorts;
  uint32_t local_id = portnum-4;
  return static_cast<int32_t>( base_id + local_id );
}


#if 0
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
#endif
