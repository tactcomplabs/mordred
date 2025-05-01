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

  std::string rtr_prefix   = "rtr_port";
  std::string rtr_linkname = rtr_prefix + "0";
  portnum                   = 0;
  while( isPortConnected( rtr_linkname ) ) {
    SST::Link* link =
      configureLink( rtr_linkname, new Event::Handler2<SimpleRTR, &SimpleRTR::handleRtrInWithID, int>( this, portnum ) );

    if( !link )
      output.fatal( CALL_INFO, -1, "Error in %s: unable to configure link %s\n", getName().c_str(), rtr_linkname.c_str() );

    RtrPortsVec.push_back( link );

    // Build the next name to check
    portnum++;
    rtr_linkname = rtr_prefix + std::to_string( portnum );
  }

  num_local_ports = (uint32_t) LocalPortsVec.size();
  num_rtr_ports  = (uint32_t) RtrPortsVec.size();

  // Load subcomponents
  topology = loadUserSubComponent<TopologyAPI>( "topology", ComponentInfo::SHARE_NONE, cid, num_rtr_ports, num_local_ports );
  if ( !topology )
    output.fatal( CALL_INFO, -1, "Couldn't load topology\n" );


  output.verbose(
    CALL_INFO, 5, 0, "Constructor complete for %s. local_ports=%" PRIu32 "; rtr_ports=%" PRIu32 "\n",
    getName().c_str(), num_local_ports, num_rtr_ports
  );
  output.flush();
}

void SimpleRTR::init( uint32_t phase ) {
  output.verbose(CALL_INFO, 5, 0, "SimpleRTR::init(%" PRIu32 ")\n", phase);
  output.flush();

  topology->init( phase );
  MordredFlit* ev = nullptr;

  while ( ( ev = topology->sendInitMessage() ) != nullptr ) {

  }

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


void SimpleRTR::handleLocalInWithID( SST::Event* ev, int32_t linknum ) {
  MordredFlit* mev = static_cast<MordredFlit*>( ev );
  if( mev ) {
    output.verbose( CALL_INFO, 5, 0, "SimpleRTR::handleLocalInWithID on link %" PRId32 "\n", linknum );
    delete mev;
  } else {
    output.fatal( CALL_INFO, -1, "Error! Bad mev type received by %s on link ID %" PRId32 "\n", getName().c_str(), linknum );
  }
}

void SimpleRTR::handleRtrInWithID( SST::Event* ev, int32_t linknum ) {
  MordredFlit* mev = static_cast<MordredFlit*>( ev );
  if( mev ) {
    output.verbose( CALL_INFO, 5, 0, "SimpleRTR::handleTopoInWithID on link %" PRId32 "\n", linknum );
    delete mev;
  } else {
    output.fatal( CALL_INFO, -1, "Error! Bad mev type received by %s on link ID %" PRId32 "\n", getName().c_str(), linknum );
  }
}
