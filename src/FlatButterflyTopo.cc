//
// FlatButterflyTopo.cc
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
#include "FlatButterflyTopo.h"

using namespace SST::Mordred;

FlatButterflyTopo::FlatButterflyTopo( ComponentId_t id, Params& params,
  uint32_t rtr_id, uint32_t num_ports, uint32_t num_local_ports ) :
  TopologyAPI( id ),
  rtrId( rtr_id ),
  numPorts( num_ports ),
  numLocalPorts( num_local_ports )
{
  const auto verbosity = params.find<uint32_t>( "verbose", 5 );
  output               = new Output( "FlatButterflyTopo [" + getName() + ":@p:@t]:", verbosity, 0, Output::STDOUT );

  output->verbose( CALL_INFO, 1, 0, "FlatButterflyTopo constructed; rtr_id=%" PRIu32 ", num_ports=%" PRIu32 ", local_ports=%" PRIu32 "\n",
    rtrId, numPorts, numLocalPorts);
}

int32_t FlatButterflyTopo::getEndpointId( uint32_t portnum ) {
  output->fatal( CALL_INFO, -1, "Not yet implemented \n" );
  return 0;
}

uint32_t FlatButterflyTopo::routePacket( uint32_t dest ) {
  uint32_t dest_port = 0;

  output->fatal( CALL_INFO, -1, "Not yet implemented \n" );

  //output->verbose( CALL_INFO, 5, 0, "Routing: dest=%" PRIu32 ", dest_rtr_id=%" PRIu32 ", dest_x=%" PRIu32 ", dest_y=%" PRIu32 "\n",
  //  dest, dest_rtr_id, dest_x, dest_y );

  if ( dest_port >= numPorts )
    output->fatal( CALL_INFO, -1, "Error! Invalid destination for packet; numPorts=%" PRIu32 ", dest_port=%" PRIu32 "\n",
      numPorts, dest_port);
  return dest_port;
}