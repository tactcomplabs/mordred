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

int32_t MeshTopology::getEndpointId( uint32_t portnum ) {
  if ( portnum < 4 )
    return -1;
  uint32_t base_id = rtrId * numLocalPorts;
  uint32_t local_id = portnum-4;
  return static_cast<int32_t>( base_id + local_id );
}
