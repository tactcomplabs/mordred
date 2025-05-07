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

  // TODO: Ensure numPorts >= numLocalPorts

  // Load subcomponents
  topology = loadUserSubComponent<TopologyAPI>( "topology", ComponentInfo::SHARE_NONE, id, numPorts, numLocalPorts );
  if ( !topology )
    output.fatal( CALL_INFO, -1, "Couldn't load topology\n" );

  // Configure local/endpt ports -- borrowed this approach from
  // sst-elements/src/sst/elements/simpleElementExample/basicLinks.cc
  for ( uint32_t i = 0; i < numPorts; i++ ) {
    std::string linkname = "port" + std::to_string(i);
    if ( isPortConnected( linkname ) ) {
      portsVec.push_back( loadAnonymousSubComponent<RtrPortControlAPI>("mordred.rtrPortControl", "portcontrol", (int)i,
        ComponentInfo::SHARE_PORTS, params, topology, id, i) );
    } else {
      output.verbose( CALL_INFO, 9, 0, "Port %u with name=%s unconnected\n", i, linkname.c_str() );
      portsVec.push_back( nullptr );
    }
  }

  output.verbose(
    CALL_INFO, 5, 0, "Constructor complete for %s. local_ports=%" PRIu32 "; rtr_ports=%" PRIu32 "\n",
    getName().c_str(), numLocalPorts, numPorts-numLocalPorts
  );
  output.flush();
}

void SimpleRTR::init( uint32_t phase ) {
  output.verbose(CALL_INFO, 5, 0, "SimpleRTR::init(%" PRIu32 ")\n", phase);
  output.flush();

  topology->init( phase );
  for ( auto &port : portsVec )
    if ( port != nullptr )
      port->init( phase );

#if 0

  if (phase == 0) {
    uint32_t cntr = 0;
    for( const auto &i : RtrPortsVec ) {
      auto *bev = topology->sendInitMessage();
      if (!bev)
        output.fatal( CALL_INFO, -1, "Yikes! cntr=%" PRIu32 "\n", cntr );
      i->sendUntimedData( bev );
      cntr++;
    }
    output.verbose( CALL_INFO, 5, 0, "Sent %" PRIu32 " init msgs\n", cntr );
  }

  if ( phase >= 1 ) {
    for ( size_t i = 0; i < RtrPortsVec.size(); i++ ) {
      auto ev = (RtrPortsVec[i]->recvUntimedData());
      topology->processInitMessage( i, ev );
      //output.verbose( CALL_INFO, 5, 0, "Received Untimed packet with src_name \n");//,
      //  bev->src_name.c_str() );
    }
  }
#endif
}

void SimpleRTR::setup() {

}

void SimpleRTR::complete( uint32_t phase ) {

}

void SimpleRTR::finish() {

}

bool SimpleRTR::clockTick( Cycle_t cycle ) {
  output.verbose( CALL_INFO, 3, 0, "Cycle=%" PRIu64 "\n", cycle );
  return false;
}



void SimpleRTR::handleInEvent( SST::Event* ev, int32_t linknum ) {
  MordredFlit* mev = static_cast<MordredFlit*>( ev );
  if( mev ) {
    output.verbose( CALL_INFO, 5, 0, "SimpleRTR::handleInEvent on link %" PRId32 "\n", linknum );
    delete mev;
  } else {
    output.fatal( CALL_INFO, -1, "Error! Bad mev type received by %s on link ID %" PRId32 "\n", getName().c_str(), linknum );
  }
}
