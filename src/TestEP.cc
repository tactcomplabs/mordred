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

#include "TestEP.h"
#include "MordredEvents.h"

using namespace SST;
using namespace SST::Mordred;

TestEP::TestEP( ComponentId_t cid, Params& params ) : Component( cid ) {

  auto Verbosity = params.find<uint32_t>( "verbose", 5 );
  // Initialize the output handler
  output.init( "TestEP[" + getName() + ":@p:@t]: ", Verbosity, 0, SST::Output::STDOUT );

  auto clockFreq = params.find<std::string>("clock", "1GHz");
  timeConverter = registerClock( clockFreq, new Clock::Handler2<TestEP, &TestEP::clockTick>(this) );
  registerAsPrimaryComponent();
  primaryComponentDoNotEndSim();

  nocIface = loadUserSubComponent<Interfaces::SimpleNetwork>( "noc_iface", ComponentInfo::SHARE_NONE, 1 );
  if ( !nocIface )
    output.fatal( CALL_INFO, -1, "Failed to load nocIface\n" );

  // TODO: Add parameters to configure as needed
  //output.setVerboseMask( DEBUG_INIT_PHASE );

  output.verbose( CALL_INFO, 5, 0, "Constructor complete for %s \n", getName().c_str() );
}

void TestEP::init( const uint32_t phase ) {
  output.verbose( CALL_INFO, 5, DEBUG_INIT_PHASE, "TestEP::init(%" PRIu32 ")\n", phase );
  nocIface->init( phase );
}

void TestEP::setup() {
  nocIface->setup();
}

void TestEP::complete( const uint32_t phase ) {
  nocIface->complete( phase );
}

void TestEP::finish() {
  nocIface->finish();
}


bool TestEP::clockTick( Cycle_t cycle ) {
  //if ( nocIface->getEndpointID() == 0 )
  //  output.verbose( CALL_INFO, 3, 0, "Tick; Cycle=%" PRIu64 "\n", cycle );

  if ( ( nocIface->getEndpointID() == 0 ) && ( cycle == 10 ) ) {
    output.verbose( CALL_INFO, 3, 0, "Sending packet. Cycle=%" PRIu64 "\n", cycle );
    auto pkt = new simpleTestEvent( "howdy");

    auto *req = new Interfaces::SimpleNetwork::Request();
    req->src = nocIface->getEndpointID();
    req->dest = 1;
    req->size_in_bits = 8*(sizeof(simpleTestEvent) + pkt->str.size());
    req->vn = 0;
    req->givePayload( pkt );

    nocIface->send( req, 0 );
  }

#if 0
  // Simple output testing - need at least 9 endpoints as currently written
  // TODO: Check if the routing still works if >1 endpt per router
  if ( ( nocIface->getEndpointID() == 0 ) && ( cycle == 20 ) ) {
    output.verbose( CALL_INFO, 3, 0, "Sending packet. Cycle=%" PRIu64 "\n", cycle );
    auto pkt = new simpleTestEvent( "howdy");

    auto *req = new Interfaces::SimpleNetwork::Request();
    req->src = nocIface->getEndpointID();
    req->dest = 3;
    req->size_in_bits = 8*(sizeof(simpleTestEvent) + pkt->str.size());
    req->vn = 0;
    req->givePayload( pkt );

    nocIface->send( req, 0 );
  }

  if ( ( nocIface->getEndpointID() == 0 ) && ( cycle == 30 ) ) {
    output.verbose( CALL_INFO, 3, 0, "Sending packet. Cycle=%" PRIu64 "\n", cycle );
    auto pkt = new simpleTestEvent( "howdy");

    auto *req = new Interfaces::SimpleNetwork::Request();
    req->src = nocIface->getEndpointID();
    req->dest = 7;
    req->size_in_bits = 8*(sizeof(simpleTestEvent) + pkt->str.size());
    req->vn = 0;
    req->givePayload( pkt );

    nocIface->send( req, 0 );
  }
#endif

  // Poll NOC for requests
  Interfaces::SimpleNetwork::Request *req = nocIface->recv( 0 );
  if ( req ) {
    auto *tev = static_cast<simpleTestEvent*>(req->takePayload());
    output.verbose( CALL_INFO, 5, 0, "Processing flit at cycle=%" PRIu64 "; printing str=%s\n", cycle, tev->str.c_str() );
    delete req; // done with request
    delete tev; // done with event
  }

  // End simulation
  if ( cycle == 60 ) {
    output.verbose( CALL_INFO, 3, 0, "Cycle=%" PRIu64 "\n", cycle );
    primaryComponentOKToEndSim();
  }
  return false;
}

#if 0
void TestEP::handleIncomingPacket( SST::Event* ev ) {
  MordredFlit* mev = static_cast<MordredFlit*>( ev );
  if( mev ) {
    output.verbose( CALL_INFO, 5, 0, "TestEP::handle in packet\n" );
    delete mev;
  } else {
    output.fatal( CALL_INFO, -1, "Error! Bad mev type received by %s\n", getName().c_str() );
  }
}
#endif
