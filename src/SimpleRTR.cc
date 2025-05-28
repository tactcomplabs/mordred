//
// SimpleRTR.cc
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#include <string>
#include <cinttypes>
#include <cstdint>

#include "SimpleRTR.h"
#include "MordredEvents.h"

using namespace SST;
using namespace SST::Mordred;

SimpleRTR::SimpleRTR( ComponentId_t cid, Params& params ) : Component( cid ) {

  auto Verbosity = params.find<uint32_t>( "verbose", 5 );
  // Initialize the output handler
  output.init( "SimpleRTR[" + getName() + ":@p:@t]: ", Verbosity, 0, SST::Output::STDOUT );

  id = params.find<uint32_t>("id",UINT32_MAX);
  if ( id == UINT32_MAX ) {
    output.fatal(CALL_INFO, -1, "SimpleRTR requires id to be specified\n");
  }

  auto clockFreq = params.find<std::string>("clock", "1GHz");
  timeConverter = registerClock( clockFreq, new Clock::Handler2<SimpleRTR, &SimpleRTR::clockTick>(this) );

  numPorts = params.find<uint32_t>("num_ports", 3);
  numLocalPorts = params.find<uint32_t>( "num_local_ports", 1 );
  numVns = params.find<uint32_t>( "num_vns", 1 ); // commented out as an ELI param for now
  numVcs = params.find<uint32_t>( "num_vcs", 1 );

  if ( numPorts <= numLocalPorts )
    output.fatal( CALL_INFO, -1, "num_ports must be greater than num_local_ports\n" );

  // Load subcomponents
  topology = loadUserSubComponent<TopologyAPI>( "topology", ComponentInfo::SHARE_NONE, id, numPorts, numLocalPorts );
  if ( !topology )
    output.fatal( CALL_INFO, -1, "Couldn't load topology\n" );

  arbWinners.resize( numPorts, std::make_pair( UINT32_MAX, UINT32_MAX ) );
  perPortVnObjs.resize( numPorts );
  // Configure local/endpt ports -- borrowed this approach from
  // sst-elements/src/sst/elements/simpleElementExample/basicLinks.cc
  for ( uint32_t i = 0; i < numPorts; i++ ) {
    std::string linkname = "port" + std::to_string(i);
    if ( isPortConnected( linkname ) ) {
      perPortVnObjs.at(i).resize( numVns ); // TODO: This may want to vary based on if the port is a local endpoint or a router connection
      for ( uint32_t j = 0; j < numVns; j++ ) {
        perPortVnObjs.at(i).at(j).allocateVecs( numVcs ); // TODO: This may want to vary based on if the port is a local endpoint or a router connection
        perPortVnObjs.at(i).at(j).initVecs();
      }
      portsVec.push_back( loadAnonymousSubComponent<RtrPortControlAPI>("mordred.rtrPortControl", "portcontrol", (int)i,
        ComponentInfo::SHARE_PORTS, params, topology, &perPortVnObjs[i], id, i) );
    } else {
      output.verbose( CALL_INFO, 5, 0, "Port %u with name=%s unconnected\n", i, linkname.c_str() );
      portsVec.push_back( nullptr );
    }
  }

  arbiter = loadAnonymousSubComponent<ArbAPI>( "mordred.arbRR", "arbiter", 0, ComponentInfo::SHARE_NONE, params, &perPortVnObjs, &arbWinners );
  if (arbiter == nullptr) {
    output.fatal( CALL_INFO, -1, "arbiter is a nullptr\n" );
  }

  output.verbose(
    CALL_INFO, 5, 0, "Constructor complete for %s. local_ports=%" PRIu32 "; rtr_ports=%" PRIu32 "\n",
    getName().c_str(), numLocalPorts, numPorts-numLocalPorts
  );
  output.flush();
}

SimpleRTR::~SimpleRTR() {
  for ( auto &port : portsVec )
    if ( port != nullptr )
      delete port;
  delete arbiter;
  delete topology;
}

void SimpleRTR::init( uint32_t phase ) {
  //output.verbose(CALL_INFO, 5, 0, "SimpleRTR::init(%" PRIu32 ")\n", phase);
  //output.flush();

  topology->init( phase );
  for ( auto &port : portsVec )
    if ( port != nullptr )
      port->init( phase );
}

void SimpleRTR::setup() {
  topology->setup();
  for ( auto &port : portsVec )
    if ( port != nullptr )
      port->setup();
}

bool SimpleRTR::clockTick( Cycle_t cycle ) {
  // May want/need to look at how we want to time/order ticking the ports and running the crossbar/arbitration here

  //output.verbose( CALL_INFO, 3, 0, "Cycle=%" PRIu64 "\n", cycle );
  arbiter->arbitrate();

  // For all router ports, see if we can move a flit through the "crossbar"
  for ( uint32_t i = 0; i < numPorts; i++ ) {
    if ( arbWinners[i].first == UINT32_MAX )
      continue;

    // get flit out of the input buffer of the receiving port
    MordredFlit *flit = portsVec.at( i )->getInBufFlit( arbWinners[i] );

    // send flit to the output buffer of the sending port
    // TODO: Make sure flit->next_port exists
    portsVec.at( flit->next_port )->sendOutBufFlit( flit, arbWinners[i] );
  }

  for ( auto &port : portsVec ) {
    if (port == nullptr) continue;
    port->ClockTick( cycle );
  }

  return false;
}
