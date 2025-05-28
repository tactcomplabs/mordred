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

  output->verbose( CALL_INFO, 1, 0, "MeshTopology constructed; rtr_id=%" PRIu32 ", num_ports=%" PRIu32 ", local_ports=%" PRIu32 "\n",
    rtrId, numPorts, numLocalPorts);
}

int32_t MeshTopology::getEndpointId( uint32_t portnum ) {
  if ( portnum < 4 )
    return -1;
  uint32_t base_id = rtrId * numLocalPorts;
  uint32_t local_id = portnum-4;
  return static_cast<int32_t>( base_id + local_id );
}

// TODO: Set up which VC to use; local port always uses VC==0
uint32_t MeshTopology::routePacket( uint32_t dest ) {
  uint32_t dest_x = dest % xDim;
  uint32_t dest_y = dest / xDim;

  // Currently just going along x until we hit the proper y
  // then we'll route along the y.
  if ( dest_x < xId )
    return PortDirE::WEST;
  if ( dest_x > xId )
    return PortDirE::EAST;
  if ( dest_y < yId )
    return PortDirE::SOUTH;
  if ( dest_y > yId )
    return PortDirE::NORTH;

  // TODO: Account for multiple local ports
  if ( numPorts >= 4 )
    return 4;

  output->fatal( CALL_INFO, -1, "Error! Invalid destination for packet; dest_x=%" PRIu32 ", dest_y=%" PRIu32 "\n",
    dest_x, dest_y);

  return UINT32_MAX;

}