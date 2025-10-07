//
// Torus2dTopo.cc
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
#include "Torus2dTopo.h"

using namespace SST::Mordred;

TorusTopo::TorusTopo( ComponentId_t id, Params& params,
  uint32_t rtr_id, uint32_t num_ports, uint32_t num_local_ports, std::vector<uint32_t>* connected_ports ) :
  TopologyAPI( id ),
  rtrId( rtr_id ),
  endptZeroId( rtr_id * num_local_ports ),
  numPorts( num_ports ),
  numLocalPorts( num_local_ports ),
  perPortConnectedRtr( connected_ports )
{
  const auto verbosity = params.find<uint32_t>( "verbose", 5 );
  output               = new Output( "TorusTopo [" + getName() + ":@p:@t]:", verbosity, 0, Output::STDOUT );

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

  halfXDim = xDim / 2;
  halfYDim = yDim / 2;

  output->verbose( CALL_INFO, 1, 0, "TorusTopo constructed; rtr_id=%" PRIu32 ", num_ports=%" PRIu32 ", local_ports=%" PRIu32 "\n",
    rtrId, numPorts, numLocalPorts);
}

void TorusTopo::init( uint32_t phase ) {
  //output->verbose( CALL_INFO, 5, 0, "TorusTopo::init(%" PRIu32 ")\n", phase );
  //output->flush();
  if ( phase != 3)
    return;
  // Verify that every rtr-rtr port is connected
  for (uint32_t i = 0; i < ( numPorts - numLocalPorts ) ; ++i) {
    if ( perPortConnectedRtr->at( i ) == UINT32_MAX )
      output->fatal( CALL_INFO, -1, "TorusTopo: port=%" PRIu32 " unconnected\n", i );
  }
}

void TorusTopo::setup() {
#if 0
  for (uint32_t i = 0; i < numPorts; ++i) {
    output->verbose( CALL_INFO, 5, 0, "perPortConnectedRtr[%" PRIu32 "]=%" PRIu32 "\n", i, perPortConnectedRtr->at(i) );
  }
#endif
}

int32_t TorusTopo::getEndpointId( uint32_t portnum ) {
  if ( portnum < 4 )
    return -1;
  uint32_t base_id = rtrId * numLocalPorts;
  uint32_t local_id = portnum-4;
  return static_cast<int32_t>( base_id + local_id );
}

uint32_t TorusTopo::routePacket( uint32_t dest ) {
  uint32_t dest_rtr_id = dest / numLocalPorts;

  // Not stopping here
  if ( dest_rtr_id != rtrId ) {
    uint32_t dest_x = dest_rtr_id % xDim;
    uint32_t dest_y = dest_rtr_id / xDim;

    // Currently just going along x until we hit the proper y
    // then we'll route along the y.
    // Routing logic from merlin
    PortDirE outport;
    if ( dest_x != xId ) {
      // do X routing
      int32_t dx_neg = (int32_t)xId - (int32_t)dest_x;
      if ( dx_neg < 0 ) dx_neg += (int32_t)xDim;
      int32_t dx_pos = (int32_t)dest_x - (int32_t)xId;
      if ( dx_pos < 0 ) dx_pos += (int32_t)xDim;

      if ( dx_pos <= dx_neg )
        outport = PortDirE::EAST;
      else
        outport = PortDirE::WEST;
    } else {
      // do Y routing
      int32_t dy_neg = (int32_t)yId - (int32_t)dest_y;
      if ( dy_neg < 0 ) dy_neg += (int32_t)yDim;
      int32_t dy_pos = (int32_t)dest_y - (int32_t)yId;
      if ( dy_pos < 0 ) dy_pos += (int32_t)yDim;

      if ( dy_pos <= dy_neg )
        outport = PortDirE::NORTH;
      else
        outport = PortDirE::SOUTH;
    }
    //output->verbose( CALL_INFO, 5, 0, "Routing: dest_rtr_id=%" PRIu32 ", outport=%" PRIu32 "\n",
    //  dest_rtr_id, (uint32_t)outport);
    //output->flush();
    return outport;
  }

  // Local packet, just find the dest port
  uint32_t dest_port = dest - endptZeroId; // this should be the number of the local port
  dest_port += 4; // add 4 to account for router ports
  if ( dest_port >= numPorts )
    output->fatal( CALL_INFO, -1, "Error! Invalid destination for packet; numPorts=%" PRIu32 ", dest_port=%" PRIu32 "\n",
      numPorts, dest_port);
  //output->verbose( CALL_INFO, 5, 0, "Local packet; dest_port=%" PRIu32 "\n", dest_port);
  //output->flush();
  return dest_port;
}
