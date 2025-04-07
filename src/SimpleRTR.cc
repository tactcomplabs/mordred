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

#include "SimpleRTR.h"
#include "MordredEvents.h"

using namespace SST;
using namespace SST::Mordred;

SimpleRTR::SimpleRTR( ComponentId_t cid, Params& params ) : Component( cid ) {

  auto Verbosity = params.find<uint32_t>( "verbose", 5 );
  // Initialize the output handler
  output.init( "SimpleRTR[" + getName() + ":@p:@t]: ", Verbosity, 0, SST::Output::STDOUT );

  // Configure local/endpt ports -- borrowed this approach from
  // sst-elements/src/sst/elements/simpleElementExample/basicLinks.cc
  std::string lcl_prefix   = "local_port";
  std::string lcl_linkname = lcl_prefix + "0";
  int32_t     portnum      = 0;
  while( isPortConnected( lcl_linkname ) ) {
    SST::Link* link =
      configureLink( lcl_linkname, new Event::Handler2<SimpleRTR, &SimpleRTR::handleLocalInWithID, int>( this, portnum ) );

    if( !link )
      output.fatal( CALL_INFO, -1, "Error in %s: unable to configure link %s\n", getName().c_str(), lcl_linkname.c_str() );

    LocalPortsVec.push_back( link );

    // Build the next name to check
    portnum++;
    lcl_linkname = lcl_prefix + std::to_string( portnum );
  }

  std::string topo_prefix   = "topo_port";
  std::string topo_linkname = topo_prefix + "0";
  portnum                   = 0;
  while( isPortConnected( topo_linkname ) ) {
    SST::Link* link =
      configureLink( topo_linkname, new Event::Handler2<SimpleRTR, &SimpleRTR::handleTopoInWithID, int>( this, portnum ) );

    if( !link )
      output.fatal( CALL_INFO, -1, "Error in %s: unable to configure link %s\n", getName().c_str(), topo_linkname.c_str() );

    TopoPortsVec.push_back( link );

    // Build the next name to check
    portnum++;
    topo_linkname = topo_prefix + std::to_string( portnum );
  }

  num_local_ports = (uint32_t) LocalPortsVec.size();
  num_topo_ports  = (uint32_t) TopoPortsVec.size();

  output.verbose(
    CALL_INFO, 5, 0, "Constructor complete for %s. local_ports=%" PRIu32 "; topo_ports=%" PRIu32 "\n",
    getName().c_str(), num_local_ports, num_topo_ports
  );
  output.flush();
}

void SimpleRTR::init( uint32_t phase ) {
  output.verbose(CALL_INFO, 5, 0, "SimpleRTR::init(%" PRIu32 ")\n", phase);
  output.flush();

  if (phase == 0) {
    auto *bev = new basicMordredEvent();
    bev->src_name = getName();

    for ( const auto &i : TopoPortsVec )
      i->sendUntimedData( bev );
  }

  if ( phase >= 1 ) {
    for ( const auto &i : TopoPortsVec ) {
      basicMordredEvent *bev = static_cast<basicMordredEvent*>(i->recvUntimedData());
      output.verbose( CALL_INFO, 5, 0, "Received Untimed packet with src_name %s\n",
        bev->src_name.c_str() );
    }
  }
}

void SimpleRTR::setup() {

}

void SimpleRTR::complete( uint32_t phase ) {

}

void SimpleRTR::finish() {

}


void SimpleRTR::handleLocalInWithID( SST::Event* ev, int32_t linknum ) {
  basicMordredEvent* mev = static_cast<basicMordredEvent*>( ev );
  if( mev ) {
    output.verbose( CALL_INFO, 5, 0, "SimpleRTR::handleLocalInWithID on link %" PRId32 "\n", linknum );
    delete mev;
  } else {
    output.fatal( CALL_INFO, -1, "Error! Bad mev type received by %s on link ID %" PRId32 "\n", getName().c_str(), linknum );
  }
}

void SimpleRTR::handleTopoInWithID( SST::Event* ev, int32_t linknum ) {
  basicMordredEvent* mev = static_cast<basicMordredEvent*>( ev );
  if( mev ) {
    output.verbose( CALL_INFO, 5, 0, "SimpleRTR::handleTopoInWithID on link %" PRId32 "\n", linknum );
    delete mev;
  } else {
    output.fatal( CALL_INFO, -1, "Error! Bad mev type received by %s on link ID %" PRId32 "\n", getName().c_str(), linknum );
  }
}
